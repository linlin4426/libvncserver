#include "codec_api.h"
#include "common.h"
#include "mmalh264_encoder.h"

static bool encoderInitialized = false;
static u_int32_t frameWidth = 0;
static u_int32_t frameHeight = 0;
static rfbClientPtr client = NULL;
static u_char *yuv_buffer = NULL;
static u_int32_t bytesSent = 0;
static u_int32_t bytesPacket = 0;
static bool openFrame = false;

static u_int32_t frameNum = 0;
static u_int64_t startTsNs = 0;
static u_int64_t lastSkipTsNs = 0;
static u_int64_t lastSendTsNs = 0;
static uint64_t desiredFrameDurationNs = 33333333; //fps

static FILE *fid = NULL;

extern void handle_frame(MMAL_BUFFER_HEADER_T *bufferHeader) {

    //DEBUG stream to file:
//    if(fid == NULL) {
//        fid = fopen("dump.h264", "wb");
//    }
//    fwrite(bufferHeader->data, bufferHeader->length, 1, fid);
//
//    char filename[20];
//    sprintf(filename, "dump%04d", frameNum);
//    FILE *fid2 = fopen(filename,"wb");
//    fwrite(bufferHeader->data, bufferHeader->length, 1, fid2);
//    fclose(fid2);

    if(bufferHeader->flags & MMAL_BUFFER_HEADER_FLAG_NAL_END) {
        client->rfbStatistics.encode_ts_end_ms = (uint32_t)(getCaptureTimeNs()/1000000);
        client->rfbStatistics.tx_ts_start_ms = (uint32_t)(getCaptureTimeNs()/1000000);
        bytesPacket += bufferHeader->length;
        bytesSent += bufferHeader->length;
        if(openFrame) {
            //send completed frame (including already queued data)
            sendOrQueueData(client, bufferHeader->data, bufferHeader->length, true);
        } else {
            //send complete frame
            sendFramebufferUpdateMsg2(client, 0, 0, frameWidth, frameHeight, bufferHeader->data, bufferHeader->length, true);
        }
        openFrame = false;

//        fprintf(stdout, "%d (%d bytes), total=%d, %dms\n",frameNum++, bytesPacket, bytesSent, client->rfbStatistics.encode_ts_end_ms - client->rfbStatistics.encode_ts_start_ms);
        client->rfbStatistics.last_frame++;
        client->rfbStatistics.tx_ts_end_ms = (uint32_t)(getCaptureTimeNs()/1000000);
        rfbSendStatistics(client);
        bytesPacket = 0;
    } else {
        if(openFrame) {
            //queue another partial frame
            sendOrQueueData(client, bufferHeader->data, bufferHeader->length, false);
        } else {
            //queue first partial frame
            sendFramebufferUpdateMsg2(client, 0, 0, frameWidth, frameHeight, bufferHeader->data, bufferHeader->length, false);
        }
        openFrame = true;
        bytesPacket = bufferHeader->length;
    }
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

extern rfbBool rfbSendFrameEncodingMmalH264(rfbClientPtr cl) {
    rfbBool result = TRUE;
    int w = cl->screen->width;
    int h = cl->screen->height;
    int rv;
    int frameSize;
    uint32_t nowTsMs = 0;
    uint32_t lastSendTsMs = 0;

    if(!encoderInitialized) {
        if(!initMmalH264(cl)) {
            cl->rfbStatistics.last_frame = 0;
            result = FALSE;
            goto error;
        }
        encoderInitialized = true;
        startCaptureStatistics();
    }

    mmalh264_encoder_encode(client, w, h, (onFrameCb)&handle_frame);

    error:
    return result;
}

void rfbMmalCleanup(rfbScreenInfoPtr screen) {
    if (encoderInitialized) {
        mmalh264_encoder_stop();
    }
    free(yuv_buffer);
}