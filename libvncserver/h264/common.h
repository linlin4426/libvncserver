#ifndef LIBVNCSERVER_COMMON_H
#define LIBVNCSERVER_COMMON_H

#include "rfb/rfb.h"

rfbBool sendFramebufferUpdateMsg(rfbClientPtr cl, int x, int y, int w, int h, u_char *data, size_t size);
rfbBool sendFramebufferUpdateMsg2(rfbClientPtr cl, int x, int y, int w, int h, u_char *data, size_t size, int forceFlush);
rfbBool sendOrQueueData(rfbClientPtr cl, u_char* data, int size, int forceFlush);
void fillInputBuffer(char *buffer, int i, int frame_width, int frame_height);

void rgba2Yuv(uint8_t *destination, uint8_t *rgb, size_t width, size_t height);

#endif //LIBVNCSERVER_COMMON_H
