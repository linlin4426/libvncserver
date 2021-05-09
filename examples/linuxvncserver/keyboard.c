#include "keyboard.h"

#include <linux/uinput.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <rfb/rfb.h>

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

int initKeyboard() {
    rfbBool result = TRUE;
    struct uinput_setup usetup;
    struct uinput_abs_setup absSetup;

    fid = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(fid == -1) {
        fprintf(stderr, "Could not open /dev/uinput. Permissions might be insufficient.\n");
        result = FALSE;
        goto error;
    }

    /* enable mouse button left and relative events */
    ioctl(fid, UI_SET_EVBIT, EV_KEY);
    for(int key = 0; key < 0x278; key++) {
        ioctl(fid, UI_SET_KEYBIT, key);
    }


    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234; /* sample vendor */
    usetup.id.product = 0x5678; /* sample product */
    strcpy(usetup.name, "Example keyboard");

    ioctl(fid, UI_DEV_SETUP, &usetup);
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

void keyDown(int key) {
    emit(fid, EV_KEY, key, 1);
    emit(fid, EV_SYN, SYN_REPORT, 0);
}

void keyUp(int key) {
    emit(fid, EV_KEY, key, 0);
    emit(fid, EV_SYN, SYN_REPORT, 0);
}

int destroyKeyboard() {
    //Give userspace some time to read the events before we destroy
    sleep(1);
    ioctl(fid, UI_DEV_DESTROY);
    close(fid);
    return 0;
};
