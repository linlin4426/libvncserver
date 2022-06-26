#include <stdlib.h>
#include <jni.h>
#include <android/log.h>

/* libvncserver */
#include "rfb/rfb.h"
#include "rfb/keysym.h"
#include "rfb/rfbregion.h"
#include "logging.h"

void fillInputBuffer(char *buffer, int i, int frame_width, int frame_height, int mousex, int mousey) {
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

void paintCursor(char *buffer, int i, int frame_width, int frame_height, int mousex, int mousey) {
    const int cursor_size = 30;
    u_int32_t stride = frame_width * 4;
    for(int y = mousey-cursor_size; y < mousey+cursor_size; y++) {
        for(int x = mousex-cursor_size; x < mousex+cursor_size; x++) {
            buffer[(y*stride)+(x*4)] = 0;
            buffer[(y*stride)+(x*4)+1] = 0;
            buffer[(y*stride)+(x*4)+2] = 0;
        }
    }
}

static int counter = 0;
//static int width = 4000;
//static int height = 3000;
static int width = 1920;
static int height = 1080;
//static int width = 1024;
//static int height = 600;

rfbScreenInfoPtr rfbScreen = NULL;

JNIEXPORT void JNICALL
Java_com_example_hstream_CaptureService_startOpenH264Encoder(JNIEnv *env, jobject thiz) {
    {
        start_logger("h264encoder");

        int argc = 0;
        char *argv[] = {};
        rfbScreen = rfbGetScreen(&argc, argv, width, height, 8, 3, 4);
        rfbScreen->frameBuffer = calloc(width * height * 4, 1);
        fillInputBuffer(rfbScreen->frameBuffer, 10, width, height, 100, 100);
        rfbInitServer(rfbScreen);

//        {
//            int i;
//            for (i = 0; rfbIsActive(rfbScreen); i++) {
//                if (rfbScreen->clientHead &&
//                    rfbScreen->clientHead->preferredEncoding == rfbEncodingX264) {
//                    if (rfbScreen->cursorX > 3 && rfbScreen->cursorX < width - 3 &&
//                        rfbScreen->cursorY > 3 && rfbScreen->cursorY < height - 3) {
//                        paintCursor(rfbScreen->frameBuffer, 10, width, height, rfbScreen->cursorX,
//                                    rfbScreen->cursorY);
//                    } else {
//                        paintCursor(rfbScreen->frameBuffer, 10, width, height, 100, 100);
//                    }
//                }
//                rfbProcessEvents(rfbScreen, 33333);
//                counter++;
//            }
//        }
    }
}

JNIEXPORT void JNICALL
Java_com_example_hstream_CaptureService_encode(JNIEnv *env, jobject thiz, jint width, jint height, jobject buffer) {
    uint8_t * image = (uint8_t*) (*env)->GetDirectBufferAddress(env, buffer);
    if(image == NULL) {
        __android_log_write(ANDROID_LOG_INFO, "encoder", "image is null");
        return;
    }

    if (rfbScreen->clientHead && rfbScreen->clientHead->preferredEncoding == rfbEncodingX264) {
        rfbScreen->frameBuffer = (char*)image;
        rfbProcessEvents(rfbScreen, 33333);
    } else {
        rfbProcessEvents(rfbScreen, 33333);
    }
}