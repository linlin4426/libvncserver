#ifndef LIBVNCSERVER_MOUSE_H
#define LIBVNCSERVER_MOUSE_H

int initMouse();
int setMousePosition(int x, int y);
void setButton(int32_t buttonMask, int down);
int destroyMouse();

#endif //LIBVNCSERVER_MOUSE_H
