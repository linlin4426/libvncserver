#include <stdlib.h>
#include <signal.h>
#include "rfb/rfb.h"
#include "rfb/keysym.h"
#include "rfb/rfbregion.h"
#include "mouse.h"

static int done = 0;
//static int width = 1366;
//static int height = 768;
static int width = 1920;
static int height = 1080;
//static int width = 1920;
//static int height = 540;

void signal_handler(int n) {
    if(n == SIGINT) done = 1;
}

typedef struct framebuffers {
    u_int8_t *fb1;
    u_int8_t *fb2;
} framebuffers;

//void *dmaCopyThread( struct framebuffers *fbs ) {
//    initDmaCopy(width, height, &(fbs->fb1));
//    while(1) {
//        dmaCopy();
////        usleep(33333);
//    }
//}
//
//void *memCopyThread( struct framebuffers *fbs ) {
//    while(1) {
//        memcpy(fbs->fb2, fbs->fb1, 1920*1080*4);
//        usleep(16666);
//    }
//}

//pthread_t thread1, thread2;
//int result1;
//int result2;
//u_int8_t *frameBuffer1 = calloc(1, 1920*1080*4);
//u_int8_t *frameBuffer2 = calloc(1, 1920*1080*4);
//struct framebuffers fbs;
//fbs.fb1 = frameBuffer1;
//fbs.fb2 = frameBuffer2;
//result1 = pthread_create( &thread1, NULL, dmaCopyThread, &fbs);
//result2 = pthread_create( &thread2, NULL, memCopyThread, &fbs);

int main(int argc,char** argv)
{
    signal(SIGINT, signal_handler);

//    //initialize display
//    MappedRegion mappedRegion = getXvfbBuffer();
//    char* inputFrameBuffer = mappedRegion->addr;
//    if(!inputFrameBuffer) {
//        fprintf(stderr, "Couldn't open fbp.");
//        goto error;
//    }

    char* inputFrameBuffer = calloc(1, 1920*1080*4);

    if(initMouse() == FALSE) {
        fprintf(stderr, "Couldn't not init mouse.\n");
        goto error;
    }

    rfbScreenInfoPtr rfbScreen = rfbGetScreen(&argc, argv, width, height, 8, 3, 4);
    rfbScreen->frameBuffer = (char*)inputFrameBuffer;
    rfbInitServer(rfbScreen);
    printf("initialized!\n");
    while(rfbIsActive(rfbScreen) && !done) {
        setMousePosition(rfbScreen->cursorX, rfbScreen->cursorY);
        rfbProcessEvents(rfbScreen, 16666);
    }

error:
    destroyMouse();
    return(0);
}