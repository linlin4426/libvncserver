#ifndef LIBVNCSERVER_KEYBOARD_H
#define LIBVNCSERVER_KEYBOARD_H

int initKeyboard();
void keyDown(int key);
void keyUp(int key);
int destroyKeyboard();

#endif //LIBVNCSERVER_KEYBOARD_H
