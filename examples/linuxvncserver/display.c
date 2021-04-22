#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <rpicopy.h>
#include <rpimemmgr.h>
#include "display.h"
#include "X11/XWDFile.h"


//use with xvfb (virtual framebuffer):
//Xvfb :99 -screen 0 1920x1080x24 -fbdir /var/tmp
//export DISPLAY=:99
//lxterminal
//x2x -to :99 -from :0 (run on x)
//lxsession
// /dev/input/mouse0

MappedRegion getXvfbBuffer() {
    struct stat stat_buf;
    int res;
    int fid;

    MappedRegion reg = (MappedRegion)malloc( sizeof( struct mappedRegionS));

    res = stat( "/var/tmp/Xvfb_screen0", &stat_buf);
    if (res < 0) {
        fprintf(stderr, "Error: cannot stat /var/tmp/Xvfb_screen0");
        exit(1);
    }

    printf("%d", sizeof(XWDFileHeader));
    reg->length = stat_buf.st_size;

    /* open mapped file */
    fid = open( "/var/tmp/Xvfb_screen0", O_RDWR, S_IRUSR | S_IWUSR);
    if (fid < 0) {
        fprintf(stderr, "Error: cannot open /var/tmp/Xvfb_screen0");
        exit(1);
    }

    reg->addr = mmap( NULL, reg->length, PROT_READ | PROT_WRITE, MAP_SHARED, fid, 0) + 3200;
    reg->length -= 3200;
    close(fid);
    if (reg->addr == (void *)-1) {
        fprintf(stderr, "Error: cannot map /var/tmp/Xvfb_screen0 to memory\n");
        exit(1);
    }

    printf("Mapped /var/tmp/Xvfb_screen0. (length=%d)\n", reg->length);

    return reg;
}


///* screen size in bytes */
///* x * y * bpp / 8 */
unsigned long int screensize;

fbp_t mapFramebufferToMemory() {

  /* structures for important fb information */
  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;

  /* Open framebuffer */
  fbfd = open("/dev/fb0", O_RDWR);

  if(fbfd == -1)
  {
    printf("Error: cannot open framebuffer device");
    exit(1);
  }

  /* Set the tty to graphics mode */
  /* Get fixed screen information */
  if(ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1)
  {
    printf("Error reading fixed information");
    exit(2);
  }

  /* Get variable screen information */
  if(ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1)
  {
    printf("Error reading variable information");
    exit(3);
  }

  printf("%s\n", finfo.id);
  printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

  screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8 ;
  printf("%lu bytes\n", screensize);

  fbp = (fbp_t)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

  if ((int)fbp == -1)
  {
    printf("Error: failed to map framebuffer device to memory\n");
    exit(4);
  }

  /* Attempt to open the tty and set it to graphics mode */
//  ttyfd = open("/dev/tty1", O_RDWR);
//  if (ttyfd == -1) {
//    printf("Error: could not open the tty\n");
//  }else{
//    ioctl(ttyfd, KDSETMODE, KD_GRAPHICS);
//  }

  return fbp;
}

void unmapFramebuffer(fbp_t fbp) {
  /* Unmap the memory and release all the files */
  munmap(fbp, screensize);

  if (ttyfd != -1)
  {
    ioctl(ttyfd, KDSETMODE, KD_TEXT);
    close(ttyfd);
  }

  close(fbfd);
}

static struct rpimemmgr st;
static u_int32_t width = 0;
static u_int32_t height = 0;
static u_int32_t frameSize = 0;
static uint32_t src_addr, dst_addr;

void initDmaCopy(int width, int height, u_char **frameBuffer) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    fbfd = open("/dev/fb0", O_RDWR);
    if(ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        printf("Error reading fixed information");
        exit(2);
    }

    frameSize = width*height*4;

    src_addr = finfo.smem_start;

    if (rpicopy_init())
        fprintf(stderr,"rpicopy_init\n");
    if (rpimemmgr_init(&st))
        fprintf(stderr, "rpimemmgr_init\n");
    if (rpimemmgr_alloc_vcsm(frameSize, 1, VCSM_CACHE_TYPE_NONE, (void**) frameBuffer, &dst_addr, &st))
        fprintf(stderr, "rpimemmgr_alloc_vcsm\n");

    memcpy_dma_config(dst_addr, src_addr, frameSize, 1, 0);
}

void dmaCopy() {
    memcpy_dma(dst_addr, src_addr, frameSize);
}

void destroyDmaCopy() {
    rpicopy_finalize();
    rpimemmgr_finalize(&st);
}