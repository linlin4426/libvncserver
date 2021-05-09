#include <fcntl.h>
#include "mmalh264_encoder.h"
#include "rfb/rfb.h"
#include "interface/vcos/vcos_semaphore.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_types.h"
#include <interface/mmal/vc/mmal_vc_shm.h>
#include <bcm_host.h>
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "mmalh264_encoder.h"
#include "common.h"
//#include "timers.h"
#include "display.h"
#include "interface/vmcs_host/vc_dispmanx.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "linux/dma-buf.h"

#define CHECK_STATUS(status, msg) if (status != MMAL_SUCCESS) { fprintf(stderr, msg"\n"); goto error; }

static uint8_t codec_header_bytes[128];

static const char FRAME_BPP = 4;

/** Context for our application */
static struct CONTEXT_T {
    VCOS_SEMAPHORE_T semaphore;
    MMAL_QUEUE_T *queue;
} context;

static MMAL_COMPONENT_T *encoder = 0;
static MMAL_POOL_T *pool_in = 0;
static MMAL_POOL_T *pool_out = 0;
static unsigned int count;

void default_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if(buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED) {
        printf("MMAL_EVENT_PARAMETER_CHANGED\n");
    } else if(buffer->cmd == MMAL_EVENT_FORMAT_CHANGED) {
        printf("MMAL_EVENT_FORMAT_CHANGED\n");
    } else if(buffer->cmd == MMAL_EVENT_EOS) {
        printf("MMAL_EVENT_EOS\n");
    } else if(buffer->cmd == MMAL_EVENT_ERROR) {
        printf("MMAL_EVENT_ERROR\n");
        printf("Error type %d", *(MMAL_STATUS_T*)buffer->data);
    }

    //TODO:
//    printf("control callback\n");

//    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
//    {
//        MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
//        switch (param->hdr.id)
//        {
//            case MMAL_PARAMETER_CAMERA_SETTINGS:
//            {
//                MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T*)param;
//                vcos_log_error("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
//                               settings->exposure,
//                               settings->analog_gain.num, settings->analog_gain.den,
//                               settings->digital_gain.num, settings->digital_gain.den);
//                vcos_log_error("AWB R=%u/%u, B=%u/%u",
//                               settings->awb_red_gain.num, settings->awb_red_gain.den,
//                               settings->awb_blue_gain.num, settings->awb_blue_gain.den);
//            }
//                break;
//        }
//    }
//    else if (buffer->cmd == MMAL_EVENT_ERROR)
//    {
//        vcos_log_error("No data received from sensor. Check all connections, including the Sunny one on the camera board");
//    }
//    else
//    {
//        vcos_log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
//    }
//
    mmal_buffer_header_release(buffer);
}


static void log_mmal_es_format(MMAL_ES_FORMAT_T *format) {
    fprintf(stdout, " type: %i, fourcc: %4.4s\n", format->type, (char *) &format->encoding);
    fprintf(stdout, " bitrate: %i, framed: %i\n", format->bitrate,
            !!(format->flags & MMAL_ES_FORMAT_FLAG_FRAMED));
    fprintf(stdout, " extra data: %i, %p\n", format->extradata_size, format->extradata);
    fprintf(stdout, " width: %i, height: %i, (%i,%i,%i,%i)\n",
            format->es->video.width, format->es->video.height,
            format->es->video.crop.x, format->es->video.crop.y,
            format->es->video.crop.width, format->es->video.crop.height);
}

/** Callback from the input port.
 * Buffer has been consumed and is available to be used again. */
static void input_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    //fprintf(stdout, "input callback");
    struct CONTEXT_T *ctx = (struct CONTEXT_T *) port->userdata;

    /* The decoder is done with the data, just recycle the buffer header into its pool */
    mmal_buffer_header_release(buffer);

    /* Kick the processing thread */
    vcos_semaphore_post(&ctx->semaphore);
}

/** Callback from the output port.
 * Buffer has been produced by the port and is available for processing. */
static void output_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    //fprintf(stdout, "output callback");
    struct CONTEXT_T *ctx = (struct CONTEXT_T *) port->userdata;

    /* Queue the decoded video frame */
    mmal_queue_put(ctx->queue, buffer);

    /* Kick the processing thread */
    vcos_semaphore_post(&ctx->semaphore);
}

void h264_encoder_cleanup() {
    /* Cleanup everything */
    if (encoder)
        mmal_component_destroy(encoder);
    if (pool_in)
        mmal_pool_destroy(pool_in);
    if (pool_out)
        mmal_pool_destroy(pool_out);
    if (context.queue)
        mmal_queue_destroy(context.queue);

    vcos_semaphore_delete(&context.semaphore);
}

typedef struct MMAL_PORT_PRIVATE_T
{
    /** Pointer to the private data of the core */
    struct MMAL_PORT_PRIVATE_CORE_T *core;
    /** Pointer to the private data of the module in use */
    struct MMAL_PORT_MODULE_T *module;
    /** Pointer to the private data used by clock ports */
    struct MMAL_PORT_CLOCK_T *clock;

    MMAL_STATUS_T (*pf_set_format)(MMAL_PORT_T *port);
    MMAL_STATUS_T (*pf_enable)(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T);
    MMAL_STATUS_T (*pf_disable)(MMAL_PORT_T *port);
    MMAL_STATUS_T (*pf_send)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *);
    MMAL_STATUS_T (*pf_flush)(MMAL_PORT_T *port);
    MMAL_STATUS_T (*pf_parameter_set)(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param);
    MMAL_STATUS_T (*pf_parameter_get)(MMAL_PORT_T *port, MMAL_PARAMETER_HEADER_T *param);
    MMAL_STATUS_T (*pf_connect)(MMAL_PORT_T *port, MMAL_PORT_T *other_port);

    uint8_t *(*pf_payload_alloc)(MMAL_PORT_T *port, uint32_t payload_size);
    void     (*pf_payload_free)(MMAL_PORT_T *port, uint8_t *payload);

} MMAL_PORT_PRIVATE_T;

uint8_t *custom_dmabuf_allocator(MMAL_PORT_T *port, uint32_t payload_size) {
    int dma_buf_fd = -1;
    uint32_t fb_id = 0xce; //with hardware cursor
//    uint32_t fb_id = 0xcc;
//    uint32_t fb_id = 0x60;

    char *card = "/dev/dri/card0";
    const int drmfd = open(card, O_RDONLY);
    if (drmfd < 0) {
        fprintf(stderr, "Cannot open card\n");
        goto error;
    }
    drmModeFBPtr fb = drmModeGetFB(drmfd, fb_id);
    if (!fb) {
        fprintf(stderr, "Cannot open fb %#x\n", fb_id);
        goto error;
    }

    printf("fb_id=%#x width=%u height=%u pitch=%u bpp=%u depth=%u handle=%#x\n",
           fb_id, fb->width, fb->height, fb->pitch, fb->bpp, fb->depth, fb->handle);

    const int ret = drmPrimeHandleToFD(drmfd, fb->handle, 0, &dma_buf_fd);
    printf("drmPrimeHandleToFD = %d, fd = %d\n", ret, dma_buf_fd);

    if(vcsm_init() != 0) {
        printf("Could not init vcsm\n");
        goto error;
    }

    unsigned int vcsm_handle = vcsm_import_dmabuf(dma_buf_fd, "/dev/dri/card0");
    printf("vcsm_handle = %d\n", vcsm_handle);

    //https://github.com/6by9/drm_mmal/blob/master/drm_mmal.c
    //assign this to pool header data?
    uint8_t *buffer = vcsm_vc_hdl_from_hdl(vcsm_handle);
    return buffer;
error:
    vcsm_free(vcsm_handle);
//    vcsm_exit();
    if (dma_buf_fd >= 0)
        close(dma_buf_fd);
    if (fb)
        drmModeFreeFB(fb);
    close(drmfd);
    return 0;
}

int mmalh264_encoder_init(int frame_width, int frame_height) {
    MMAL_STATUS_T status = MMAL_SUCCESS;
    MMAL_PORT_T *encoder_output = NULL;
    MMAL_PORT_T *encoder_input = NULL;
    MMAL_ES_FORMAT_T *format_out = NULL;
    MMAL_ES_FORMAT_T *format_in = NULL;

    vcos_semaphore_create(&context.semaphore, "example", 1);

    /* Create the encoder component.
     * This specific component exposes 2 ports (1 input and 1 output). Like most components
     * its expects the format of its input port to be set by the client in order for it to
     * know what kind of data it will be fed. */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);
    CHECK_STATUS(status, "failed to create decoder");

    /* Set format of video decoder input port */
    format_in = encoder->input[0]->format;
    format_in->type = MMAL_ES_TYPE_VIDEO;
//    format_in->encoding = MMAL_ENCODING_RGB16;
    format_in->encoding = MMAL_ENCODING_BGRA;
    //format_in->encoding = MMAL_ENCODING_RGBA;
    format_in->es->video.width = frame_width;
    format_in->es->video.height = frame_height;
    format_in->es->video.frame_rate.num = 30;
    format_in->es->video.frame_rate.den = 1;
    format_in->es->video.par.num = 1;
    format_in->es->video.par.den = 1;
    format_in->es->video.crop.x = 0;
    format_in->es->video.crop.y = 0;
    format_in->es->video.crop.width = frame_width;
    format_in->es->video.crop.height = frame_height;

    /* If the data is known to be framed then the following flag should be set:
     * format_in->flags |= MMAL_ES_FORMAT_FLAG_FRAMED; */

    status = mmal_port_format_commit(encoder->input[0]);
    CHECK_STATUS(status, "failed to commit input port format");

    format_out = encoder->output[0]->format;
    format_out->type = MMAL_ES_TYPE_VIDEO;
    format_out->encoding = MMAL_ENCODING_H264;
    format_out->es->video.width = frame_width;
    format_out->es->video.height = frame_height;
    format_out->es->video.frame_rate.num = 30;
    format_out->es->video.frame_rate.den = 1;
    format_out->es->video.par.num = 1;
    format_out->es->video.par.den = 1;
    status = mmal_port_format_commit(encoder->output[0]);
    CHECK_STATUS(status, "failed to commit output port format");

    encoder_input = encoder->input[0];

    //configure zero copy mode on input
    mmal_port_parameter_set_boolean(encoder_input, MMAL_PARAMETER_ZERO_COPY, 1);

    //additional parameters
    mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_MINIMISE_FRAGMENTATION, 1);
//    mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_H264_DISABLE_CABAC, 1);
//    mmal_port_parameter_set_boolean(encoder_input, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, 1);

    //configure h264 encoding
    //see https://raw.githubusercontent.com/raspberrypi/userland/master/host_applications/linux/apps/raspicam/RaspiVid.c
    encoder_output = encoder->output[0];

    {
        MMAL_PARAMETER_VIDEO_PROFILE_T param;
        param.hdr.id = MMAL_PARAMETER_PROFILE;
        param.hdr.size = sizeof(param);
        param.profile[0].profile = MMAL_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE;
        param.profile[0].level = MMAL_VIDEO_LEVEL_H264_42;
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        CHECK_STATUS(status, "failed to set port parameter MMAL_PARAMETER_PROFILE");
    }

    //seems to make image crisper and increases framerate ?
    {
        MMAL_PARAMETER_UINT32_T param = {{MMAL_PARAMETER_VIDEO_ENCODE_H264_DEBLOCK_IDC, sizeof(param)}, 1};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        CHECK_STATUS(status, "failed to set port parameter MMAL_PARAMETER_VIDEO_ENCODE_H264_DEBLOCK_IDC");
    }

    //quality (quantisationParameter 0..51)
    {
        MMAL_PARAMETER_UINT32_T param = {{MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)}, 30};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        CHECK_STATUS(status, "failed to set port parameter MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT");
    }

    {
        MMAL_PARAMETER_UINT32_T param = {{MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param)}, 30};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        CHECK_STATUS(status, "failed to set port parameter MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT");
    }

    {
        MMAL_PARAMETER_UINT32_T param = {{MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param)}, 30};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        CHECK_STATUS(status, "failed to set port parameter MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT");
    }

    {
        MMAL_PARAMETER_UINT32_T param = {{MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, 300};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        CHECK_STATUS(status, "failed to set port parameter MMAL_PARAMETER_INTRAPERIOD");
    }

    {
        MMAL_PARAMETER_BOOLEAN_T param = {{MMAL_PARAMETER_VIDEO_ENCODE_HEADERS_WITH_FRAME, sizeof(param)}, 0};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        CHECK_STATUS(status, "failed to set port parameter MMAL_PARAMETER_VIDEO_ENCODE_HEADERS_WITH_FRAME");
    }

    /* Display the input port format */
    fprintf(stdout, "Input port format for %s\n", encoder->input[0]->name);
    log_mmal_es_format(encoder->input[0]->format);

    /* Display the input port format */
    fprintf(stdout, "Output port format for %s\n", encoder->output[0]->name);
    log_mmal_es_format(encoder->output[0]->format);

    /* The format of both ports is now set so we can get their buffer requirements and create
     * our buffer headers. We use the buffer pool API to create these. */
    encoder->input[0]->buffer_num = encoder->input[0]->buffer_num_min;
    //encoder->input[0]->buffer_size = FRAME_WIDTH * FRAME_HEIGHT * FRAME_BPP;
    encoder->input[0]->buffer_size = encoder->input[0]->buffer_size_recommended;
    encoder->output[0]->buffer_num = encoder->output[0]->buffer_num_min;
    //encoder->output[0]->buffer_size = FRAME_WIDTH * FRAME_HEIGHT * FRAME_BPP;
    encoder->output[0]->buffer_size = encoder->output[0]->buffer_size_recommended * 5;
    mmal_port_enable(encoder->control, default_control_callback);

    encoder->input[0]->priv->pf_payload_alloc = custom_dmabuf_allocator;
    pool_in = mmal_port_pool_create(encoder->input[0], encoder->input[0]->buffer_num, encoder->input[0]->buffer_size);

//    pool_in = mmal_pool_create(encoder->input[0]->buffer_num,
//                               encoder->input[0]->buffer_size);
    pool_out = mmal_pool_create(encoder->output[0]->buffer_num,
                                encoder->output[0]->buffer_size);

//  fprintf(stdout, "Created pools:\n");
//  fprintf(stdout, "encoder->input[0]->buffer_num=%d\n", encoder->input[0]->buffer_num);
//  fprintf(stdout, "encoder->input[0]->buffer_size=%d\n", encoder->input[0]->buffer_size);
//  fprintf(stdout, "encoder->output[0]->buffer_num=%d\n", encoder->output[0]->buffer_num);
//  fprintf(stdout, "encoder->output[0]->buffer_size=%d\n", encoder->output[0]->buffer_size);

    /* Create a queue to store our encoded frames. The callback we will get when
     * a frame has been encoded will put the frame into this queue. */
    context.queue = mmal_queue_create();

    /* Store a reference to our context in each port (will be used during callbacks) */
    encoder->input[0]->userdata = ((void *) &context);
    encoder->output[0]->userdata = ((void *) &context);

    /* Enable all the input port and the output port.
     * The callback specified here is the function which will be called when the buffer header
     * we sent to the component has been processed. */
    status = mmal_port_enable(encoder->input[0], input_callback);
    CHECK_STATUS(status, "failed to enable input port");
    status = mmal_port_enable(encoder->output[0], output_callback);
    CHECK_STATUS(status, "failed to enable output port");

    /* Component won't start processing data until it is enabled. */
    status = mmal_component_enable(encoder);
    CHECK_STATUS(status, "failed to enable component");

    error:
    if (status != MMAL_SUCCESS) h264_encoder_cleanup();
    return status == MMAL_SUCCESS ? 0 : -1;
}

int mmalh264_encoder_encode(rfbClientPtr client, int width, int height, onFrameCb on_frame_cb) {
    MMAL_STATUS_T status = MMAL_SUCCESS;

    MMAL_BUFFER_HEADER_T *bufferHeader;

    /* Wait for bufferHeader headers to be available on either of the encoder ports */
    vcos_semaphore_wait(&context.semaphore);

    /* Send data to decode to the input port of the video encoder */
    if ((bufferHeader = mmal_queue_get(pool_in->queue)) != NULL) {
        bufferHeader->length = width * height * 3;
        bufferHeader->offset = 0;
        bufferHeader->pts = bufferHeader->dts = MMAL_TIME_UNKNOWN;
        bufferHeader->flags = MMAL_BUFFER_HEADER_FLAG_EOS;


        client->rfbStatistics.encode_ts_start_ms = (uint32_t)(getCaptureTimeNs()/1000000);
        status = mmal_port_send_buffer(encoder->input[0], bufferHeader);
        CHECK_STATUS(status, "failed to send bufferHeader\n");
        //if(!encode_timer) encode_timer = start_timer("encode");
    }

    /* Get our encoded frames */
    while ((bufferHeader = mmal_queue_get(context.queue)) != NULL) {
        /* We have a frame, do something with it (why not display it for instance?).
         * Once we're done with it, we release it. It will automatically go back
         * to its original pool so it can be reused for a new video frame.
         */

//      printf("flag = %d\n",(bufferHeader->flags));
//      printf("MMAL_BUFFER_HEADER_FLAG_EOS: %d\n",!!(bufferHeader->flags & MMAL_BUFFER_HEADER_FLAG_EOS));
//      printf("MMAL_BUFFER_HEADER_FLAG_FRAME_START: %d\n",!!(bufferHeader->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START));
//      printf("MMAL_BUFFER_HEADER_FLAG_FRAME_END: %d\n",!!(bufferHeader->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END));
//      printf("MMAL_BUFFER_HEADER_FLAG_FRAME: %d\n",!!(bufferHeader->flags & MMAL_BUFFER_HEADER_FLAG_FRAME));
//      printf("MMAL_BUFFER_HEADER_FLAG_KEYFRAME: %d\n\n",!!(bufferHeader->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME));
        on_frame_cb(bufferHeader);
        mmal_buffer_header_release(bufferHeader);
    }

    /* Send empty buffers to the output port of the decoder */
    while ((bufferHeader = mmal_queue_get(pool_out->queue)) != NULL) {
        status = mmal_port_send_buffer(encoder->output[0], bufferHeader);
        CHECK_STATUS(status, "failed to send bufferHeader");
    }

    error:
    if (status != MMAL_SUCCESS) h264_encoder_cleanup();
    return status == MMAL_SUCCESS ? 0 : -1;
}

int mmalh264_encoder_stop() {
    MMAL_STATUS_T status = MMAL_SUCCESS;

    /* Stop decoding */
    fprintf(stderr, "stop encoding\n");

    /* Stop everything. Not strictly necessary since mmal_component_destroy()
     * will do that anyway */
    mmal_port_disable(encoder->input[0]);
    mmal_port_disable(encoder->output[0]);
    mmal_component_disable(encoder);

    error:
    h264_encoder_cleanup();
    return status == MMAL_SUCCESS ? 0 : -1;
}
