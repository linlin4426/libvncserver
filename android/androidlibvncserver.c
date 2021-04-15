#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaCrypto.h>
#include <rfb/rfb.h>
#include "logging.h"

static rfbScreenInfoPtr rfbScreen;
static char* tag = "androidlibvncserve";

//TODO: move this to utils
int64_t getTimeNsec() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t) now.tv_sec*1000000000LL + now.tv_nsec;
}

//TODO: move this to utils
static void fillInputBuffer(uint8_t *buffer, int i, int frame_width, int frame_height) {
    //generate sample image data
    u_int32_t offset = 0;
    u_int32_t stride = frame_width * 4;
    for(int y = 0; y < frame_height; y++) {
        for(int x = 0; x < frame_width; x++) {
            buffer[offset+0] = (u_char)(x + (y % 255) % 255);
            buffer[offset+1] = (u_char)((x + i + ((y + i) % 255)) % 255);
            buffer[offset+2] = (u_char)((x + i*2 + ((y + i*2) % 255)) % 255);

            buffer[offset+3] = 255;
            offset += 4;
        }
    }
}

//TODO: move this to utils
static void rgba2Yuv(uint8_t *destination, uint8_t *rgba, size_t width, size_t height)
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
                uint8_t r = rgba[4 * i];
                uint8_t g = rgba[4 * i + 1];
                uint8_t b = rgba[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;

                destination[upos++] = ((-38*r + -74*g + 112*b) >> 8) + 128;
                destination[vpos++] = ((112*r + -94*g + -18*b) >> 8) + 128;

                r = rgba[4 * i];
                g = rgba[4 * i + 1];
                b = rgba[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
        else
        {
            for( x = 0; x < width; x += 1 )
            {
                uint8_t r = rgba[4 * i];
                uint8_t g = rgba[4 * i + 1];
                uint8_t b = rgba[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
    }
}


JNIEXPORT void JNICALL
Java_com_example_hstream_CaptureService_startServer(JNIEnv *env, jobject thiz) {
    static int counter = 0;
    static int width = 1920;
    static int height = 1080;
    char* argv[] = {};
    int argc = 0;


    start_logger(tag);
    rfbScreen = rfbGetScreen(&argc, argv, width, height, 8, 3, 4);
    rfbScreen->frameBuffer=calloc(width * height * 4, 1);
//    fillInputBuffer(rfbScreen->frameBuffer, 10, width, height);
    rfbInitServer(rfbScreen);

    {
        int i;
        for(i=0; rfbIsActive(rfbScreen); i++) {
            if(rfbScreen->clientHead && rfbScreen->clientHead->preferredEncoding == rfbEncodingX264) {
                fillInputBuffer(rfbScreen->frameBuffer, counter, width, height);
            }
            rfbProcessEvents(rfbScreen, 33333/10);
            counter++;
        }
    }
}

JNIEXPORT void JNICALL
Java_com_example_hstream_CaptureService_stopServer(JNIEnv *env, jobject thiz) {
    free(rfbScreen->frameBuffer);
}


static int w = 1920;
static int h = 1080;
static AMediaCodec *encoder = NULL;

JNIEXPORT void JNICALL
Java_com_example_hstream_CaptureService_initEncoder(JNIEnv *env, jobject thiz) {
    ANativeWindow* surface = NULL;
    AMediaCrypto * crypto = NULL;

    uint32_t flags = AMEDIACODEC_CONFIGURE_FLAG_ENCODE;

    AMediaFormat *format = AMediaFormat_new();

//    AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_BIT_RATE,9999999);
//    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, 60);
//    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7f420888);
//    AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_WIDTH,w);
//    AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_HEIGHT,h);
//    AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_MAX_WIDTH,w);
//    AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_MAX_HEIGHT,h);
//    AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_I_FRAME_INTERVAL,500);
//    AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_STRIDE,w);
//    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME,"video/avc");
    //https://developer.android.com/reference/android/media/MediaCodecInfo.CodecProfileLevel#AVCProfileConstrainedBaseline
    //AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_LEVEL, 65536);
    //AMEDIAFORMAT_KEY_INTRA_REFRESH_PERIOD
    //AMEDIAFORMAT_KEY_I_FRAME_INTERVAL

    encoder = AMediaCodec_createEncoderByType("video/avc");
    media_status_t mediaStatus = AMediaCodec_configure(encoder, format, surface, crypto, flags);
    if(mediaStatus == AMEDIA_OK) {
        __android_log_print(ANDROID_LOG_INFO, tag, "encoder configured.");
    } else {
        __android_log_print(ANDROID_LOG_INFO, tag, "error: encoder configure error %d", mediaStatus);
    }

    mediaStatus = AMediaCodec_start(encoder);
    if(mediaStatus == AMEDIA_OK) {
        __android_log_print(ANDROID_LOG_INFO, tag, "encoder started.");
    } else {
        __android_log_print(ANDROID_LOG_INFO, tag, "error: encoder start error %d", mediaStatus);
    }
}

JNIEXPORT void JNICALL
Java_com_example_hstream_CaptureService_startEncoder(JNIEnv *env, jobject thiz) {
    int count = 0;
    int ibuf_idx = 0;
    int obuf_idx = 0;
    size_t frame_size = w*h*3/2;
    size_t input_buffer_size = 0;
    size_t output_buffer_size = 0;
    AMediaCodecBufferInfo buffer_info;
    uint8_t *buffer_rgba = calloc(1, w*h*4);
    uint8_t *buffer_yuv420 = calloc(1, w*h*3/2);
    int64_t timeout = 3333;

    while(count < 100) {

        ibuf_idx = AMediaCodec_dequeueInputBuffer(encoder, timeout);
        if(ibuf_idx >= 0) {
            uint8_t *ibuf = AMediaCodec_getInputBuffer(encoder, ibuf_idx, &input_buffer_size);
            if(ibuf != NULL) {
                __android_log_print(ANDROID_LOG_INFO, tag, "got input buffer of size %d, queuing data.", input_buffer_size);
            }
            //generate some data.
            fillInputBuffer(buffer_rgba, 10, w, h);
            rgba2Yuv(ibuf, buffer_rgba, w, h);

            //enqueue a buffer
            AMediaCodec_queueInputBuffer(encoder, ibuf_idx, 0, frame_size, getTimeNsec(), 0);
            count++;
        }

        //dequeue all pending buffer (wait 33.3ms for it)
        obuf_idx = AMediaCodec_dequeueOutputBuffer(encoder, &buffer_info, timeout);
        while(obuf_idx >= 0) {
            __android_log_print(ANDROID_LOG_INFO, tag, "got output buffer: size=%d", buffer_info.size);
            AMediaCodec_getOutputBuffer(encoder, obuf_idx, &output_buffer_size);
            //TODO: do something with thee data - extract nalus for example and send.
            AMediaCodec_releaseOutputBuffer(encoder, obuf_idx, FALSE);
            obuf_idx = AMediaCodec_dequeueOutputBuffer(encoder, &buffer_info, timeout);
        }
    }
}



//https://cpp.hotexamples.com/examples/-/-/AMediaCodec_queueInputBuffer/cpp-amediacodec_queueinputbuffer-function-examples.html
//FULL example:
//    ANativeWindow *nativeWindow;
//    AMediaCodec_createInputSurface(encoder, &nativeWindow);
//    AMediaCodec_setInputSurface()
//    AMediaCodec_setOutputSurface()

//AMediaCodec_queueInputBuffer(encoder, )
//  AMediaCodec_stop(encoder)

//JNIEXPORT jstring JNICALL
//Java_com_example_hstream_CaptureService_stringFromJNI(
//        JNIEnv* env,
//        jobject /* this */) {
//    std::string hello =;
//    return env->NewStringUTF(hello.c_str());
//}

/*

extern "C"
JNIEXPORT void JNICALL
Java_com_example_mango2_CaptureService_startMjpegServer(JNIEnv *env, jobject thiz, jint port) {
    mjpegServerOpen(port);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_mango2_CaptureService_encode(JNIEnv *env, jobject thiz, jint width, jint height, jobject buffer) {
    using namespace mango;
    __android_log_write(ANDROID_LOG_INFO, "encoder", "before getDirectBufferAddress");
    u8* image = (u8*) env->GetDirectBufferAddress(buffer);
    if(image == nullptr) {
        __android_log_write(ANDROID_LOG_INFO, "encoder", "image is null");
        return;
    }

    ImageEncoder imageEncoder(".jpg");
    ImageEncodeOptions encodeOptions;
    encodeOptions.quality = 0.01f * (float)25;
    Surface sourceSurface(width, height, Format(32, Format::UNORM, Format::RGBA, 8, 8, 8, 8), width*4, image);
    MemoryStream memoryStream(encoderOutputBuffer, width*height*3);
    memoryStream.seek(0, mango::Stream::BEGIN);
    ImageEncodeStatus status = imageEncoder.encode(memoryStream, sourceSurface, encodeOptions);
    std::stringstream out;
    out << "size=" << memoryStream.size() << "offset=" << memoryStream.offset();
    __android_log_write(ANDROID_LOG_INFO, "encoder", out.str().c_str());
    __android_log_write(ANDROID_LOG_INFO, "encoder", "Sending...");
    mjpegServerSendFrame(memoryStream.data(), (int)(memoryStream.offset()-1));
    __android_log_write(ANDROID_LOG_INFO, "encoder", "Sent.");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_mango2_CaptureService_stopMjpegServer(JNIEnv *env, jobject thiz) {
    mjpegServerClose();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_mango2_CaptureService_startEncoder(JNIEnv *env, jobject thiz, jint width,
                                                    jint height) {
    encoderOutputBuffer = (u8*)calloc(1, width * height * 4);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_mango2_CaptureService_stopEncoder(JNIEnv *env, jobject thiz) {
    free(encoderOutputBuffer);
}


 */