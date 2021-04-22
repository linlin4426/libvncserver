#include "rfb/rfb.h"
#include "libevdev/libevdev.h"
#include "libevdev/libevdev-uinput.h"
#include "mouse_libevdev.h"

struct libevdev *dev = NULL;
struct libevdev_uinput *uidev = NULL;

int initMouse() {
    rfbBool result = TRUE;
    int err;
    char* devnode;
    char* syspath;

    dev = libevdev_new();
    libevdev_set_name(dev, "hstream virtual absolute mouse");
    libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_ABS, ABS_X, NULL);
    libevdev_enable_event_code(dev, EV_ABS, ABS_Y, NULL);
    libevdev_enable_event_type(dev, EV_ABS);

    err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);

    if(err != 0) {
        fprintf(stderr, "could not create hstream virtual mouse");
        result = FALSE;
        goto error;
    }

    libevdev_set_abs_minimum(dev, ABS_X, 0);
    libevdev_set_abs_minimum(dev, ABS_Y,0);
    libevdev_set_abs_maximum(dev, ABS_X, 1920);
    libevdev_set_abs_maximum(dev, ABS_Y, 1080);

    usleep(15000);

    devnode = libevdev_uinput_get_devnode(uidev);
    syspath = libevdev_uinput_get_syspath(uidev);

    printf("Device successfully created at %s (syspath %s)\n", devnode, syspath);

error:
    return result;
}

int setMousePosition(int x, int y) {
    printf("posting event\n");
    if(libevdev_uinput_write_event(uidev, EV_ABS, ABS_X, x) != 0) {
        fprintf(stderr, "error writing event EV_ABS/ABSX\n");
    }
    usleep(100000);
    if(libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0) != 0) {
        fprintf(stderr, "error writing event EV_SYN/SYN_REPORT\n");
    }
    usleep(100000);
    if(libevdev_uinput_write_event(uidev, EV_ABS, ABS_Y, y) != 0) {
        fprintf(stderr, "error writing event EV_ABS/ABSY\n");
    }
    usleep(100000);
    if(libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0) != 0) {
        fprintf(stderr, "error writing event EV_SYN/SYN_REPORT\n");
    }
    usleep(100000);
}

int destroyMouse() {
    libevdev_uinput_destroy(uidev);
}

/*
 *     if (rc < 0) {
        fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
        exit(1);
    }
    printf("Input device name: \"%s\"\n", libevdev_get_name(dev));
    printf("Input device ID: bus %#x vendor %#x product %#x\n",
           libevdev_get_id_bustype(dev),
           libevdev_get_id_vendor(dev),
           libevdev_get_id_product(dev));
    if (!libevdev_has_event_type(dev, EV_REL) ||
        !libevdev_has_event_code(dev, EV_KEY, BTN_LEFT)) {
        printf("This device does not look like a mouse\n");
        exit(1);
    }

    do {
        struct input_event ev;
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == 0)
            printf("Event: %s %s %d\n",
                   libevdev_event_type_get_name(ev.type),
                   libevdev_event_code_get_name(ev.type, ev.code),
                   ev.value);
    } while (rc == 1 || rc == 0 || rc == -EAGAIN);
 */

