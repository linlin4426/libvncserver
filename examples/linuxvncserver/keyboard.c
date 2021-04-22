//#include <linux/uinput.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <stdio.h>
//
//void emit(int fd, int type, int code, int val)
//{
//    struct input_event ie;
//
//    ie.type = type;
//    ie.code = code;
//    ie.value = val;
//    /* timestamp values below are ignored */
//    ie.time.tv_sec = 0;
//    ie.time.tv_usec = 0;
//
//    write(fd, &ie, sizeof(ie));
//}
//
//void setup() {
//    printf("Hello World\n");
//    struct uinput_setup usetup;
//    int i = 50;
//
//    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
//
//    /* enable mouse button left and relative events */
//    ioctl(fd, UI_SET_EVBIT, EV_KEY);
//    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
//
//    ioctl(fd, UI_SET_EVBIT, EV_REL);
//    ioctl(fd, UI_SET_RELBIT, REL_X);
//    ioctl(fd, UI_SET_RELBIT, REL_Y);
//
//    memset(&usetup, 0, sizeof(usetup));
//    usetup.id.bustype = BUS_USB;
//    usetup.id.vendor = 0x1234; /* sample vendor */
//    usetup.id.product = 0x5678; /* sample product */
//    strcpy(usetup.name, "Example device");
//
//    ioctl(fd, UI_DEV_SETUP, &usetup);
//    ioctl(fd, UI_DEV_CREATE);
//
//    /*
//     * On UI_DEV_CREATE the kernel will create the device node for this
//     * device. We are inserting a pause here so that userspace has time
//     * to detect, initialize the new device, and can start listening to
//     * the event, otherwise it will not notice the event we are about
//     * to send. This pause is only needed in our example code!
//     */
//    sleep(1);
//
//    /* Move the mouse diagonally, 5 units per axis */
//    while ()
//    for (int i = 0; i < 50; i++) {
//        emit(fd, EV_REL, REL_X, 5);
//        emit(fd, EV_REL, REL_Y, 5);
//        emit(fd, EV_SYN, SYN_REPORT, 0);
//        usleep(15000);
//    }
//}
//
///*
// * Give userspace some time to read the events before we destroy the
// * device with UI_DEV_DESTOY.
// */
//sleep(1);
//
//ioctl(fd, UI_DEV_DESTROY);
//close(fd);
//
//return 0;
//};