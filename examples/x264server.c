#include <stdlib.h>

/* libvncserver */
#include "rfb/rfb.h"
#include "rfb/keysym.h"
#include "rfb/rfbregion.h"

//void fillInputBuffer(char *buffer, int i, int frame_width, int frame_height, int mousex, int mousey) {
//  //generate sample image data
//  u_int32_t offset = 0;
//  u_int32_t stride = frame_width * 4;
//  for(int y = 0; y < frame_height; y++) {
//    for(int x = 0; x < frame_width; x++) {
//      buffer[offset+0] = (uint8_t)(x + (y % 255) % 255);
//      buffer[offset+1] = (uint8_t)((x + i + ((y + i) % 255)) % 255);
//      buffer[offset+2] = (uint8_t)((x + i*2 + ((y + i*2) % 255)) % 255);
//
//      buffer[offset+3] = 255;
//      offset += 4;
//    }
//  }
//}

void paintCursor(char *buffer, int i, int frame_width, int frame_height, int mousex, int mousey) {
    const int cursor_size = 5;
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

int main(int argc,char** argv)
{
  rfbScreenInfoPtr rfbScreen = rfbGetScreen(&argc, argv, width, height, 8, 3, 4);
  rfbScreen->frameBuffer=calloc(width * height * 4, 1);
//  fillInputBuffer(rfbScreen->frameBuffer, 10, width, height, 100, 100);
  rfbInitServer(rfbScreen);

  {
    int i;
    for(i=0; rfbIsActive(rfbScreen); i++) {
      //fprintf(stderr,"%d\r",i);
//      rfbLog("[%d/%d]\n",rfbScreen->cursorX,rfbScreen->cursorY);
      if(rfbScreen->clientHead && rfbScreen->clientHead->preferredEncoding == rfbEncodingX264) {
          if(rfbScreen->cursorX > 3 && rfbScreen->cursorX < width-3 && rfbScreen->cursorY > 3 && rfbScreen->cursorY < height-3) {
//              fillInputBuffer(rfbScreen->frameBuffer, counter, width, height, rfbScreen->cursorX, rfbScreen->cursorY);
              paintCursor(rfbScreen->frameBuffer, 10, width, height, rfbScreen->cursorX, rfbScreen->cursorY);
          } else {
//              fillInputBuffer(rfbScreen->frameBuffer, counter, width, height, 100, 100);
              paintCursor(rfbScreen->frameBuffer, 10, width, height, 100, 100);
          }
      }
      rfbProcessEvents(rfbScreen, 33333);
//      rfbMarkRectAsModified();
//      sraRegionPtr region = sraRgnCreateRect(0,0,width,height);
//      sleep(1);
//      if(rfbScreen->clientHead && rfbScreen->clientHead->preferredEncoding == rfbEncodingX264) {
//          rfbSendFramebufferUpdate(rfbScreen->clientHead, NULL);
//      }
//      rfbMarkRegionAsModified(rfbScreen, region);
//      if(rfbScreen->clientHead) {
//          rfbSendUpdateBuf(rfbScreen->clientHead);
//      }
      counter++;
    }
  }

//pthreads must be available
//  rfbRunEventLoop(rfbScreen,40000,FALSE);
//  while(1) {
//    printf("*\n");
//    fillInputBuffer(rfbScreen->frameBuffer, counter, width, height);
//    usleep(40000);
//    counter++;
//  }
  return(0);
}