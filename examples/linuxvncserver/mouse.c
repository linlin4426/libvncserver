#include <linux/uinput.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <rfb/rfb.h>
#include "mouse.h"

//Apr 19 22:12:14 pi kernel: [ 7348.869323] input: Example device as /devices/virtual/input/input27
//pi@pi:/dev/input/by-id $ cat /sys/devices/virtual/input/input27/name
//Example device

//pi@pi:/dev/input/by-id $ cat /sys/devices/virtual/input/input27/name
//Example device


//WarpPointer(display, src_w, dest_w, src_x, src_y, src_width, src_height, dest_x,
//    dest_y)
//Display *display;
//Window src_w, dest_w;
//int src_x, src_y;
//unsigned int src_width, src_height;
//int dest_x, dest_y;

//https://wiki.archlinux.org/index.php/Xorg
//
// --> https://stackoverflow.com/questions/64559499/make-xorg-recognize-libevdev-virtual-device


static int fid = -1;

static void emit(int fd, int type, int code, int val) {
    struct input_event ie;

    ie.type = type;
    ie.code = code;
    ie.value = val;
    /* timestamp values below are ignored */
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    size_t size = write(fd, &ie, sizeof(ie));
}

int initMouse() {
    rfbBool result = TRUE;
    struct uinput_setup usetup;
    struct uinput_abs_setup absSetup;

    fid = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(fid == -1) {
        fprintf(stderr, "Could not open /dev/uinput. Permissions might be insufficient.\n");
        result = FALSE;
        goto error;
    }

    /* enable mouse button left, middle, right and wheel events */
    ioctl(fid, UI_SET_EVBIT, EV_KEY);
    ioctl(fid, UI_SET_EVBIT, EV_SYN);
    ioctl(fid, UI_SET_EVBIT, EV_MSC);

    ioctl(fid, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fid, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fid, UI_SET_KEYBIT, BTN_MIDDLE);

    ioctl(fid, UI_SET_EVBIT, EV_ABS);
    ioctl(fid, UI_SET_ABSBIT, ABS_X);
    ioctl(fid, UI_SET_ABSBIT, ABS_Y);

    ioctl(fid, UI_SET_EVBIT, EV_REL);
    ioctl(fid, UI_SET_RELBIT, REL_WHEEL);

    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234; /* sample vendor */
    usetup.id.product = 0x5678; /* sample product */
    strcpy(usetup.name, "Virtual mouse");

    ioctl(fid, UI_DEV_SETUP, &usetup);

    memset(&absSetup, 0, sizeof(absSetup));
    absSetup.code = ABS_X;
    absSetup.absinfo.minimum = 0;
    absSetup.absinfo.maximum = 1920;
    ioctl(fid, UI_ABS_SETUP, &absSetup);
    absSetup.code = ABS_Y;
    absSetup.absinfo.minimum = 0;
    absSetup.absinfo.maximum = 1080;
    ioctl(fid, UI_ABS_SETUP, &absSetup);

    ioctl(fid, UI_DEV_CREATE);

    /*
     * On UI_DEV_CREATE the kernel will create the device node for this
     * device. We are inserting a pause here so that userspace has time
     * to detect, initialize the new device, and can start listening to
     * the event, otherwise it will not notice the event we are about
     * to send. This pause is only needed in our example code!
     */
    sleep(1);
error:
    return result;
}

int setMousePosition(int x, int y) {
    emit(fid, EV_ABS, ABS_X, x);
    emit(fid, EV_ABS, ABS_Y, y);
    emit(fid, EV_SYN, SYN_REPORT, 0);
}

void setButton(int32_t button, int down) {
    if(button & 1) {
        emit(fid, EV_KEY, BTN_LEFT, down ? 1 : 0);
    } else if(button & 2) {
        emit(fid, EV_KEY, BTN_MIDDLE, down ? 1 : 0);
    } else if(button & 4) {
        emit(fid, EV_KEY, BTN_RIGHT, down ? 1 : 0);
    } else if(button & 8) {
        emit(fid, EV_REL, REL_HWHEEL, 1);
    } else if(button & 16) {
        emit(fid, EV_REL, REL_HWHEEL, +1);
    }

    emit(fid, EV_SYN, SYN_REPORT, 0);
}

/*
 * Give userspace some time to read the events before we destroy the
 * device with UI_DEV_DESTOY.
 */
int destroyMouse() {
    sleep(1);
    ioctl(fid, UI_DEV_DESTROY);
    close(fid);
    return 0;
};