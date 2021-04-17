#include "codec_api.h"
#include "common.h"

static ISVCEncoder* encoder = NULL;
static u_char *yuv_buffer = NULL;

static rfbBool initOpenH264(rfbClientPtr cl) {
    rfbBool result = TRUE;
    if(cl->screen->depth != 32) {
        rfbErr("screen depth of 32 bits required for x264 encoding.\n");
        result = FALSE;
        goto error;
    }

    WelsCreateSVCEncoder(&encoder);
    SEncParamBase param;
    memset (&param, 0, sizeof (SEncParamBase));
//    param.iUsageType = SCREEN_CONTENT_REAL_TIME; //from EUsageType enum
    param.iUsageType = CAMERA_VIDEO_REAL_TIME; //from EUsageType enum
    param.fMaxFrameRate = 5;
    param.iPicWidth = cl->screen->width;
    param.iPicHeight = cl->screen->height;
    param.iTargetBitrate = 15000000;
    encoder->Initialize(&param);

//    SEncParamExt paramExt;
//    memset (&paramExt, 0, sizeof (SEncParamExt));
//    paramExt.iUsageType = CAMERA_VIDEO_REAL_TIME;
//    paramExt.fMaxFrameRate = 30;
//    paramExt.iPicWidth = cl->screen->width;
//    paramExt.iPicHeight = cl->screen->height;
//    paramExt.iTargetBitrate = 15000000;
//    paramExt.bEnableFrameSkip = 1;
//    paramExt.iSpatialLayerNum = 2;
//    paramExt.iNumRefFrame = 1;
//
//    paramExt.sSpatialLayers[0].iVideoWidth = cl->screen->width;
//    paramExt.sSpatialLayers[0].iVideoHeight = cl->screen->height/2;
//    paramExt.sSpatialLayers[0].fFrameRate = 30;
//    paramExt.sSpatialLayers[0].iSpatialBitrate = 15000000/2;
//    paramExt.sSpatialLayers[1].iVideoWidth = cl->screen->width;
//    paramExt.sSpatialLayers[1].iVideoHeight = cl->screen->height/2;
//    paramExt.sSpatialLayers[1].fFrameRate = 30;
//    paramExt.sSpatialLayers[1].iSpatialBitrate = 15000000/2;
//    encoder->InitializeExt(&paramExt);
error:
    return result;
}

static u_int64_t t0;

extern "C" rfbBool rfbSendFrameEncodingOpenH264(rfbClientPtr cl) {
    rfbBool result = TRUE;
    int w = cl->screen->width;
    int h = cl->screen->height;
    int rv;
    int frameSize;

    if(encoder == NULL) {
        if(!initOpenH264(cl)) {
            cl->rfbStatistics.last_frame = 0;
            result = FALSE;
            goto error;
        }
        t0 = getTimeNowMs();
    } else {
        cl->rfbStatistics.last_frame++;
    }

    if(yuv_buffer == NULL) {
        yuv_buffer = (u_char*)malloc(w*h + ((w*h)/2));
    }

//    if(cl->screen->width != w || cl->screen->height != param.i_height) {
//        //resize input buffer for X264 instance.
//    }

    cl->rfbStatistics.encode_ts_start_ms = (uint32_t)(getTimeNowMs() - t0);

    rgba2Yuv(yuv_buffer, (u_char*)cl->screen->frameBuffer, w, h);

    frameSize = w * h * 3 / 2;
    SFrameBSInfo info;
    memset (&info, 0, sizeof (SFrameBSInfo));
    SSourcePicture pic;
    memset (&pic, 0, sizeof (SSourcePicture));
    pic.iPicWidth = w;
    pic.iPicHeight = h;
//    pic.uiTimeStamp
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = pic.iPicWidth;
    pic.iStride[1] = pic.iStride[2] = pic.iPicWidth >> 1;
    pic.pData[0] = yuv_buffer;
    pic.pData[1] = pic.pData[0] + w * h;
    pic.pData[2] = pic.pData[1] + (w * h >> 2);
    rv = encoder->EncodeFrame(&pic, &info);

    cl->rfbStatistics.encode_ts_end_ms = (uint32_t)(getTimeNowMs() - t0);

    cl->rfbStatistics.tx_ts_start_ms = (uint32_t)(getTimeNowMs() - t0);

    if(rv == cmResultSuccess && info.eFrameType != videoFrameTypeSkip) {
        //output bitstream
        int iLayer;
        for (iLayer=0; iLayer < info.iLayerNum; iLayer++)
        {
            SLayerBSInfo* pLayerBsInfo = &info.sLayerInfo[iLayer];

            int iLayerSize = 0;
            int iNalIdx = pLayerBsInfo->iNalCount - 1;
            do {
                iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
                --iNalIdx;
            } while (iNalIdx >= 0);

            unsigned char *outBuf = pLayerBsInfo->pBsBuf;
            sendFramebufferUpdateMsg(cl, 0, 0, w, h, outBuf, iLayerSize);
        }
    }

    cl->rfbStatistics.tx_ts_end_ms = (uint32_t)(getTimeNowMs() - t0);
    rfbSendStatistics(cl);

error:
    return result;
}

void rfbH264Cleanup(rfbScreenInfoPtr screen) {
    if (encoder) {
        encoder->Uninitialize();
        WelsDestroySVCEncoder(encoder);
    }
    free(yuv_buffer);
}