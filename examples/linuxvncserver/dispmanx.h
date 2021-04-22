#ifndef LIBVNCSERVER_DISPMANX_H
#define LIBVNCSERVER_DISPMANX_H

void dispmanx_init(int w, int h, u_int8_t *buffer);
void dispmanx_copy_snapshot();
void dispmanx_destroy();

#endif //LIBVNCSERVER_DISPMANX_H
