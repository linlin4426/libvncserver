#include <stdlib.h>
#include <signal.h>
#include "rfb/rfb.h"
#include "mouse.h"
#include "keyboard.h"
#include "keymap_x11_linux.h"
#include "rfbQmuKeyboardEventProtocolExtension.h"

static int done = 0;
//static int width = 1024;
//static int height = 768;
static int width = 1920;
static int height = 1080;
//static int width = 1280;
//static int height = 1024;
//static int width = 720;
//static int height = 480;

void signal_handler(int n) {
    if(n == SIGINT) done = 1;
}

static int oldButtonMask=0;
static void doptr(int buttonMask,int x,int y,rfbClientPtr cl) {
    setMousePosition(x, y);
    printf("buttonMask = %d, x=%d, y=%d\n", buttonMask, x, y);
    for(int i = 0; i < 8; i++) {
        int button = 1<<i;
        if((buttonMask & button) && (oldButtonMask & button) == 0) {
            setButton(button, 1);
        } else if ((buttonMask & button) == 0 && (oldButtonMask & button)) {
            setButton(button, 0);
        }
    }
    oldButtonMask = buttonMask;
}

static void dokbd(rfbBool down, rfbKeySym keySym, rfbClientPtr cl) {
    if(down) {
        keyDown(code_map_x11_to_linux[keySym]);
    } else {
        keyUp(code_map_x11_to_linux[keySym]);
    }
    printf("dokbd: keySym=%d, down=%d\n", keySym, down);
    printf("dokbd: linux code=%d\n",code_map_x11_to_linux[keySym]);
}

static void SetRichCursor(rfbScreenInfoPtr rfbScreen)
{
    int i,j,w=32,h=32,bpp=4;
    /* runge */
    /*  rfbCursorPtr c = rfbScreen->cursor; */
    rfbCursorPtr c;
    char bitmap[]=
        "                                "
        "              xxxxxx            "
        "       xxxxxxxxxxxxxxxxx        "
        "      xxxxxxxxxxxxxxxxxxxxxx    "
        "    xxxxx  xxxxxxxx  xxxxxxxx   "
        "   xxxxxxxxxxxxxxxxxxxxxxxxxxx  "
        "  xxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
        "  xxxxx   xxxxxxxxxxx   xxxxxxx "
        "  xxxx     xxxxxxxxx     xxxxxx "
        "  xxxxx   xxxxxxxxxxx   xxxxxxx "
        " xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
        " xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
        " xxxxxxxxxxxx  xxxxxxxxxxxxxxx  "
        " xxxxxxxxxxxxxxxxxxxxxxxxxxxx   "
        " xxxxxxxxxxxxxxxxxxxxxxxxxxxx   "
        " xxxxxxxxxxx   xxxxxxxxxxxxxx   "
        " xxxxxxxxxx     xxxxxxxxxxxx    "
        "  xxxxxxxxx      xxxxxxxxx      "
        "   xxxxxxxxxx   xxxxxxxxx       "
        "      xxxxxxxxxxxxxxxxxxx       "
        "       xxxxxxxxxxxxxxxxxxx      "
        "         xxxxxxxxxxxxxxxxxxx    "
        "             xxxxxxxxxxxxxxxxx  "
        "                xxxxxxxxxxxxxxx "
        "   xxxx           xxxxxxxxxxxxx "
        "  xx   x            xxxxxxxxxxx "
        "  xxx               xxxxxxxxxxx "
        "  xxxx             xxxxxxxxxxx  "
        "   xxxxxx       xxxxxxxxxxxx    "
        "    xxxxxxxxxxxxxxxxxxxxxx      "
        "      xxxxxxxxxxxxxxxx          "
        "                                ";

    c=rfbMakeXCursor(w,h,bitmap,bitmap);
    c->xhot = 16; c->yhot = 24;

    c->richSource = (unsigned char*)malloc(w*h*bpp);
    if (!c->richSource) return;

    for(j=0;j<h;j++) {
        for(i=0;i<w;i++) {
            c->richSource[j*w*bpp+i*bpp+0]=i*0xff/w;
            c->richSource[j*w*bpp+i*bpp+1]=(i+j)*0xff/(w+h);
            c->richSource[j*w*bpp+i*bpp+2]=j*0xff/h;
            c->richSource[j*w*bpp+i*bpp+3]=0;
        }
    }
    rfbSetCursor(rfbScreen, c);
}
static int cursorInitialized = 0;


int main(int argc,char** argv)
{
    signal(SIGINT, signal_handler);

    if(initMouse() == FALSE) {
        fprintf(stderr, "Couldn't not init mouse.\n");
        goto error;
    }

    if(initKeyboard() == FALSE) {
        fprintf(stderr, "Couldn't not init keyboard.\n");
        goto error;
    }

    rfbProtocolExtension *qemuExtension = initQemuKeyboardEventExtenstion();
    rfbRegisterProtocolExtension(qemuExtension);

    rfbScreenInfoPtr rfbScreen = rfbGetScreen(&argc, argv, width, height, 8, 3, 4);
    rfbInitServer(rfbScreen);
    rfbScreen->ptrAddEvent = doptr;
    rfbScreen->kbdAddEvent = dokbd;
    printf("initialized!\n");
    while(rfbIsActive(rfbScreen) && !done) {
        rfbScreen->deferUpdateTime = 0;
        rfbProcessEvents(rfbScreen, 50);
//        if(rfbScreen->clientHead != NULL && !cursorInitialized) {
//            SetRichCursor(rfbScreen);
//            cursorInitialized = 1;
//        }
    }

error:
    rfbUnregisterProtocolExtension(qemuExtension);
    destroyQemuKeyboardEventExtension();
    destroyKeyboard();
    destroyMouse();
    return(0);
}