#include "codec_api.h"
#include "common.h"
#include "mmalh264_encoder.h"

static u_int64_t t0;
static bool encoderInitialized = false;
static u_int32_t frameWidth = 0;
static u_int32_t frameHeight = 0;
static rfbClientPtr client = NULL;
static u_char *yuv_buffer = NULL;
static u_int32_t frameNum = 0;

void handle_frame(MMAL_BUFFER_HEADER_T *bufferHeader) {


    int frameStartFlag = bufferHeader->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START;
    int frameEndFlag = bufferHeader->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END;

    client->rfbStatistics.encode_ts_end_ms = (uint32_t)(getTimeNowMs() - t0);

    sendFramebufferUpdateMsg2(client, 0, 0, frameWidth, frameHeight, bufferHeader->data, bufferHeader->length, 1);

//    if(frameEndFlag) {
//        client->rfbStatistics.tx_ts_start_ms = (uint32_t)(getTimeNowMs() - t0);
//    }
//
//    if(frameStartFlag) {
//        sendFramebufferUpdateMsg2(client, 0, 0, frameWidth, frameHeight, bufferHeader->data, bufferHeader->length, frameEndFlag);
//    } else {
//        sendOrQueueData(client, bufferHeader->data, bufferHeader->length, frameEndFlag);
//
//        client->rfbStatistics.encode_ts_end_ms = (uint32_t)(getTimeNowMs() - t0);
//    }
//    client->rfbStatistics.tx_ts_end_ms = (uint32_t)(getTimeNowMs() - t0);

//    fprintf(stdout, "% (%d bytes)\n",frameNum++, bufferHeader->length);

    rfbSendStatistics(client);
}

static rfbBool initMmalH264(rfbClientPtr cl) {
    frameWidth = cl->screen->width;
    frameHeight = cl->screen->height;
    client = cl;

    rfbBool result = TRUE;
    if(cl->screen->depth != 32) {
        rfbErr("screen depth of 32 bits required for x264 encoding.\n");
        result = FALSE;
        goto error;
    }

    if(mmalh264_encoder_init(cl->screen->width, cl->screen->height) != MMAL_SUCCESS) {
        rfbErr("Mmal encoder initialization failed.\n");
        result = FALSE;
        goto error;
    }

error:
    return result;
}

extern "C" rfbBool rfbSendFrameEncodingMmalH264(rfbClientPtr cl) {
    rfbBool result = TRUE;
    int w = cl->screen->width;
    int h = cl->screen->height;
    int rv;
    int frameSize;

    if(!encoderInitialized) {
        if(!initMmalH264(cl)) {
            cl->rfbStatistics.last_frame = 0;
            result = FALSE;
            goto error;
        }
        encoderInitialized = true;
        t0 = getTimeNowMs();
    } else {
        cl->rfbStatistics.last_frame++;
    }

    if(yuv_buffer == NULL) {
        yuv_buffer = (u_char*)malloc(w*h + ((w*h)/2));
    }

    cl->rfbStatistics.encode_ts_start_ms = (uint32_t)(getTimeNowMs() - t0);


//    fillInputBuffer((char*)cl->screen->frameBuffer, frameNum, w, h);

    rgba2Yuv(yuv_buffer, (u_char*)cl->screen->frameBuffer, w, h);

    mmalh264_encoder_encode(yuv_buffer, w, h, (onFrameCb)&handle_frame);

    error:
    return result;
}

void rfbMmalCleanup(rfbScreenInfoPtr screen) {
    if (encoderInitialized) {
        mmalh264_encoder_stop();
    }
    free(yuv_buffer);
}