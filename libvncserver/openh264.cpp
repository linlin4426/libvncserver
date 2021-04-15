#include "codec_api.h"
#include <rfb/rfb.h>

static ISVCEncoder* encoder = NULL;
static u_char *yuv_buffer = NULL;

static rfbBool sendOrQueueData(rfbClientPtr cl, u_char* data, int size, int forceFlush) {
    rfbBool result = TRUE;
    if (size > UPDATE_BUF_SIZE) {
        rfbErr("x264: send request size (%d) exhausts UPDATE_BUF_SIZE (%d) -> increase send buffer\n", size, UPDATE_BUF_SIZE);
        result = FALSE;
        goto error;
    }

    if(cl->ublen + size > UPDATE_BUF_SIZE) {
        if(!rfbSendUpdateBuf(cl)) {
            rfbErr("x264: could not send.\n");
            result = FALSE;
        }
    }

    memcpy(&cl->updateBuf[cl->ublen], data, size);
    cl->ublen += size;

    if(forceFlush) {
        //rfbLog("flush x264 data %d (payloadSize=%d)\n",cl->ublen,cl->ublen - sz_rfbFramebufferUpdateMsg - sz_rfbFramebufferUpdateRectHeader);
        if(!rfbSendUpdateBuf(cl)) {
            rfbErr("x264: could not send.\n");
            result = FALSE;
        }
    }

    error:
    return result;
}

static rfbBool sendFramebufferUpdateMsg(rfbClientPtr cl, int x, int y, int w, int h, u_char *data, size_t size) {
    rfbBool result = TRUE;
    rfbFramebufferUpdateMsg msg;
    rfbFramebufferUpdateRectHeader header;

    msg.type = rfbFramebufferUpdate;
    msg.pad = 0;
    msg.nRects = Swap16IfLE(1);

    if(!sendOrQueueData(cl, (u_char*)&msg, sz_rfbFramebufferUpdateMsg, 0)) {
        result = FALSE;
        goto error;
    }

    header.r.x = Swap16IfLE(x);
    header.r.y = Swap16IfLE(y);
    header.r.w = Swap16IfLE(w);
    header.r.h = Swap16IfLE(h);
    header.encoding = Swap32IfLE(rfbEncodingX264);

    rfbStatRecordEncodingSent(cl, rfbEncodingX264,
                              sz_rfbFramebufferUpdateRectHeader,
                              sz_rfbFramebufferUpdateRectHeader
                              + w * (cl->format.bitsPerPixel / 8) * h);

    if(!sendOrQueueData(cl, (u_char*)&header, sz_rfbFramebufferUpdateRectHeader, 0)) {
        result = FALSE;
        goto error;
    }

    if(!sendOrQueueData(cl, data, size, true)) {
        result = FALSE;
        goto error;
    }

    error:
    return result;
}

void rgba2Yuv(uint8_t *destination, uint8_t *rgb, size_t width, size_t height)
{
    size_t image_size = width * height;
    size_t upos = image_size;
    size_t vpos = upos + upos / 4;
    size_t i = 0;
    size_t line;
    size_t x;

    for( line = 0; line < height; ++line )
    {
        if( !(line % 2) )
        {
            for( x = 0; x < width; x += 2 )
            {
                uint8_t r = rgb[4 * i];
                uint8_t g = rgb[4 * i + 1];
                uint8_t b = rgb[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;

                destination[upos++] = ((-38*r + -74*g + 112*b) >> 8) + 128;
                destination[vpos++] = ((112*r + -94*g + -18*b) >> 8) + 128;

                r = rgb[4 * i];
                g = rgb[4 * i + 1];
                b = rgb[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
        else
        {
            for( x = 0; x < width; x += 1 )
            {
                uint8_t r = rgb[4 * i];
                uint8_t g = rgb[4 * i + 1];
                uint8_t b = rgb[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
    }
}

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
    param.fMaxFrameRate = 10;
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

extern "C" rfbBool rfbSendFrameEncodingOpenH264(rfbClientPtr cl) {
    rfbBool result = TRUE;
    int w = cl->screen->width;
    int h = cl->screen->height;
    int rv;
    int frameSize;

    if(encoder == NULL) {
        if(!initOpenH264(cl)) {
            result = FALSE;
            goto error;
        }
        //int size = x264_encoder_headers(x264, &header_nals, &header_nal_count);
    } else {
        //header_nal_count = 0;
    }

    if(yuv_buffer == NULL) {
        yuv_buffer = (u_char*)malloc(w*h + ((w*h)/2));
    }

//    if(cl->screen->width != w || cl->screen->height != param.i_height) {
//        //resize input buffer for X264 instance.
//    }

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