#include "rfbQmuKeyboardEventProtocolExtension.h"
#include "rfb/rfb.h"
#include "keyboard.h"
#include "keymap_atset1_linux.h"

typedef struct {
    uint8_t type;     /* always rfbQemuEvent */
    uint8_t subtype;  /* always 0 */
    uint16_t down;
    uint32_t keysym;  /* keysym is specified as an X keysym, may be 0 */
    uint32_t keycode; /* keycode is specified as XT key code */
} rfbQemuExtendedKeyEvent2Msg;

typedef struct {
    uint8_t type;			/* always rfbFramebufferUpdate */
    uint8_t pad;            /* 0 */
    uint16_t nRects;        /* 1 */
    rfbRectangle r;         /* 0 */
    uint32_t encoding;	    /* always rfbEncodingQemuExtendedKeyEvent */
} rfbEnableQemuKeyboardExtensionMessage;

rfbBool sendEnableQemuKeyboardExtensionMessage(rfbClientPtr cl)
{
    rfbEnableQemuKeyboardExtensionMessage msg = { 0 };

    msg.type = rfbFramebufferUpdate;
    msg.nRects = Swap16IfLE(1);
    msg.encoding = Swap32IfLE(rfbEncodingQemuExtendedKeyEvent);

    LOCK(cl->sendMutex);
    if (rfbWriteExact(cl, (char *)&msg, sizeof(rfbEnableQemuKeyboardExtensionMessage)) < 0) {
        rfbLogPerror("sendEnableQemuKeyboardExtensionMessage: write");
        rfbCloseClient(cl);
        return FALSE;
    }
    UNLOCK(cl->sendMutex);

    return TRUE;
}

rfbBool QemuKeyboardEventExtensionEnablePseudoEncoding(rfbClientPtr client, void** data, int encodingNumber) {
    if(encodingNumber == rfbEncodingQemuExtendedKeyEvent) {
        rfbLog("Enabling QEMU keyboard event protocol extension.");
        if(sendEnableQemuKeyboardExtensionMessage(client) != TRUE) {
            return FALSE;
        }
    }
    return FALSE;
}

rfbBool qemuKeyboardEventExtenstionHandleMessage(rfbClientPtr client, void* data, const rfbClientToServerMsg* message) {
    int n;
    rfbQemuExtendedKeyEvent2Msg msg;
    if ((n = rfbReadExact(client, ((char *)&msg) + 1, sz_rfbQemuExtendedKeyEventMsg - 1)) <= 0) {
        if (n != 0)
            rfbLogPerror("qemuKeyboardEventExtenstionHandleMessage: read");
        rfbCloseClient(client);
        return FALSE;
    }

    uint16_t down = Swap16IfLE(msg.down);
    uint32_t keysym = Swap32IfLE(msg.keysym);
    uint32_t keycode = Swap32IfLE(msg.keycode);

//    printf("qemuKeyboardEventExtenstion: Got a qemu message.\n");
//    printf("down=%d\n",down);
//    printf("keysym=%d\n",keysym);
//    printf("keycode=%d\n", keycode);

    //https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#74121qemu-extended-key-event-message
    uint32_t xtScancode = keycode < 0x7F ? keycode : 0xe000 | (keycode & 0x7F);

    printf("scancode=%x, linux code=%d\n",xtScancode, code_map_atset1_to_linux[xtScancode]);
    if(msg.down) {
        keyDown(code_map_atset1_to_linux[xtScancode]);
    } else {
        keyUp(code_map_atset1_to_linux[xtScancode]);
    }

    return TRUE;
}

rfbBool qemuKeyboardEventExtensionNewClient(rfbClientPtr client, void** data) {
    printf("qemuKeyboardEventExtenstion: new client.\n");
    return TRUE;
}

void qemuKeyboardEventExtensionClose(rfbClientPtr client, void* data) {
    printf("qemuKeyboardEventExtenstion: close.\n");
}

rfbProtocolExtension *qemuKeyboardEventExtension;

rfbProtocolExtension *initQemuKeyboardEventExtenstion() {
    qemuKeyboardEventExtension = calloc(1, sizeof(rfbProtocolExtension));
    qemuKeyboardEventExtension->newClient = qemuKeyboardEventExtensionNewClient;
    qemuKeyboardEventExtension->close = qemuKeyboardEventExtensionClose;
    qemuKeyboardEventExtension->enablePseudoEncoding = QemuKeyboardEventExtensionEnablePseudoEncoding;
    qemuKeyboardEventExtension->handleMessage = qemuKeyboardEventExtenstionHandleMessage;
    return qemuKeyboardEventExtension;
}

void destroyQemuKeyboardEventExtension() {
    free(qemuKeyboardEventExtension);
}
