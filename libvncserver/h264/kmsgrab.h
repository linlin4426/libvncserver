#ifndef LIBVNCSERVER_KMSGRAB_H
#define LIBVNCSERVER_KMSGRAB_H

#include <xf86drmMode.h>

typedef struct {
    int width, height;
    uint32_t fourcc;
    int fd, offset, pitch;
    int drmFd;
    drmModeFBPtr fb;
} DmaBuf;

typedef struct {
    int fb_id;
    char card[255];
    int width;
    int height;
    DmaBuf dmaBufIn;
    DmaBuf dmaBufOut;
} KMSGrabContext;

KMSGrabContext* initKmsGrab();
void runKmsGrab(KMSGrabContext *ctx);
void killKmsGrab(KMSGrabContext *ctx);

#endif //LIBVNCSERVER_KMSGRAB_H
