#ifndef LIBVNCSERVER_DISPLAY_H
#define LIBVNCSERVER_DISPLAY_H

#include "stdint.h"

struct mappedRegionS {
    void * addr;
    int length;
};

typedef struct mappedRegionS* MappedRegion;

/* framebuffer file descriptor */
int fbfd;

/* tty file descriptor */
/* if we can't open the tty because it is not writeable then we'll
   just leave it in text mode */
int ttyfd;

/* Pixel specification 16bpp */
typedef short pixel_t;

/* Framebuffer pointer */
typedef pixel_t* fbp_t;

fbp_t fbp;

fbp_t mapFramebufferToMemory();
void unmapFramebuffer(fbp_t fbp);
MappedRegion getXvfbBuffer();

/////////////////////////////////////////////////////

void initDmaCopy(int width, int height) ;
void dmaCopy(int dstAddr);
void destroyDmaCopy();

#endif //LIBVNCSERVER_DISPLAY_H
