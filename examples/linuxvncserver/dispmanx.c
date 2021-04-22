#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "display.h"
#include <linux/fb.h>
#include "interface/vmcs_host/vc_dispmanx.h"
#include "bcm_host.h"

static DISPMANX_DISPLAY_HANDLE_T display;
static DISPMANX_RESOURCE_HANDLE_T resource;
static DISPMANX_MODEINFO_T info;
static uint32_t image;
static VC_RECT_T rect;
static uint8_t *screen_buffer;
static int width;
static int height;

void dispmanx_init(int w, int h, uint8_t *buffer) {
    width = w;
    height = h;
    screen_buffer = buffer;
    bcm_host_init();
    vc_dispmanx_rect_set(&rect, 0, 0, width, height);
    display = vc_dispmanx_display_open(0);
    if(!display) {
        fprintf(stderr, "failed to open display 0\n");
    }
    resource = vc_dispmanx_resource_create(VC_IMAGE_RGBA32, width, height, &image);
    if (!resource) {
        fprintf(stderr, "Failed to create resource for snapshot\n");
    }

    screen_buffer = calloc(1, width*height*4);
}

void dispmanx_copy_snapshot() {
    if(vc_dispmanx_snapshot(display, resource, DISPMANX_NO_ROTATE)) {
        fprintf(stderr, "Failed to capture snapshot\n");
    }
    if(vc_dispmanx_resource_read_data(resource, &rect, screen_buffer, width)) {
        fprintf(stderr, "Failed to copy snapshot to resource\n");
    }
}

void dispmanx_destroy() {
    bcm_host_deinit();
}
