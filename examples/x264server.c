#include <stdlib.h>

/* libvncserver */
#include "rfb/rfb.h"
#include "rfb/keysym.h"
#include "rfb/rfbregion.h"

void fillInputBuffer(char *buffer, int i, int frame_width, int frame_height) {
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

static int counter = 0;
static int width = 1920;
static int height = 1080;
//static int width = 400;
//static int height = 300;

int main(int argc,char** argv)
{
  rfbScreenInfoPtr rfbScreen = rfbGetScreen(&argc, argv, width, height, 8, 3, 4);
  rfbScreen->frameBuffer=calloc(width * height * 4, 1);
  fillInputBuffer(rfbScreen->frameBuffer, 10, width, height);
  rfbInitServer(rfbScreen);

  sraRegionPtr region = sraRgnCreateRect(0,0,width,height);

  {
    int i;
    for(i=0; rfbIsActive(rfbScreen); i++) {
      fprintf(stderr,"%d\r",i);
      fillInputBuffer(rfbScreen->frameBuffer, counter, width, height);
      rfbProcessEvents(rfbScreen, 33333);
      //rfbMarkRectAsModified();
      rfbMarkRegionAsModified(rfbScreen, region);
      if(rfbScreen->clientHead) rfbSendUpdateBuf(rfbScreen->clientHead);
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