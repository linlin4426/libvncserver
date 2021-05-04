#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <xf86drm.h>
#include <libdrm/drm_fourcc.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <GL/gl.h>
#include "kmsgrab.h"

//https://developer.nvidia.com/blog/linking-opengl-server-side-rendering/
//https://forums.developer.nvidia.com/t/export-gl-texture-as-dmabuf/57131/2
//https://blaztinn.gitlab.io/post/dmabuf-texture-sharing/

#define OPENGL_CHECK_ERRORS { const GLenum errcode = glGetError(); if (errcode != GL_NO_ERROR) fprintf(stderr, "OpenGL Error code %i in '%s' line %i\n", errcode, __FILE__, __LINE__-1); }

#define ASSERT(cond) \
	if (!(cond)) { \
		fprintf(stderr, "ERROR @ %s:%d: (%s) failed", __FILE__, __LINE__, #cond); \
		return; \
	}

static const EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_NONE
};

static const int pbufferWidth = 9;
static const int pbufferHeight = 9;

GLfloat fullrect_vertex_buffer_data[] = {
    -0.5f, -1.0f, 0.0f, //bl
    +1.0f, -1.0f, 0.0f, //tl
    -1.0f, +1.0f, 0.0f, //br
    +1.0f, +1.0f, 0.0f,
    +1.0f, -1.0f, 0.0f,
    -1.0f, +1.0f, 0.0f
};

static const EGLint pbufferAttribs[] = {
    EGL_WIDTH, pbufferWidth,
    EGL_HEIGHT, pbufferHeight,
    EGL_NONE
};

int eglProg = -1;

static EGLDisplay eglDpy = NULL;
static EGLSurface eglSurf = NULL;
static EGLContext eglCtx = NULL;
GLuint frameBuffer;
static GLuint texIn;
static GLuint texOut;
EGLImage eglInImg;
EGLImage eglOutImg;

struct tex_storage_info {
    EGLint fourcc;
    EGLint num_planes;
    EGLuint64KHR modifiers;
    EGLint offset;
    EGLint stride;
};
static struct tex_storage_info gl_dma_info;

static KMSGrabContext kmsGrabContext;

static int initDmaBuf(KMSGrabContext* ctx) {

    printf("Opening card %s\n", ctx->card);
    const int drmfd = open(ctx->card, O_RDONLY);
    if (drmfd < 0) {
        perror("Cannot open card");
        return 1;
    }

    int dma_buf_fd = -1;
    drmModeFBPtr fb = drmModeGetFB(drmfd, ctx->fb_id);
    if (!fb) {
        printf("Cannot open fb %#x", ctx->fb_id);
        goto error;
    }

    printf("fb_id=%#x width=%u height=%u pitch=%u bpp=%u depth=%u handle=%#x",
        ctx->fb_id, fb->width, fb->height, fb->pitch, fb->bpp, fb->depth, fb->handle);

    if (!fb->handle) {
        fprintf(stderr, "Not permitted to get fb handles. Run with sudo.\n");
        goto error;
    }

    ctx->dmaBufIn.drmFd = drmfd;
    ctx->dmaBufIn.fb = fb;
    ctx->dmaBufIn.width = fb->width;
    ctx->dmaBufIn.height = fb->height;
    ctx->dmaBufIn.pitch = fb->pitch;
    ctx->dmaBufIn.offset = 0;
    ctx->dmaBufIn.fourcc = DRM_FORMAT_XRGB8888; // FIXME

    const int ret = drmPrimeHandleToFD(drmfd, fb->handle, 0, &dma_buf_fd);
    printf("drmPrimeHandleToFD = %d, fd = %d", ret, dma_buf_fd);
    ctx->dmaBufIn.fd = dma_buf_fd;

    ctx->dmaBufOut.width = fb->width;
    ctx->dmaBufOut.height = fb->height;
    ctx->dmaBufOut.pitch = fb->pitch;
    ctx->dmaBufOut.offset = 0;
    ctx->dmaBufOut.fourcc = DRM_FORMAT_XRGB8888; // FIXME

    return 0;
error:
    if (dma_buf_fd >= 0)
        close(dma_buf_fd);
    if (fb)
        drmModeFreeFB(fb);
    close(drmfd);
    return 0;
}

static void killDmaBuf(KMSGrabContext* ctx) {
    if (ctx->dmaBufIn.fd >= 0)
        close(ctx->dmaBufIn.fd);
    if (ctx->dmaBufIn.fb)
        drmModeFreeFB(ctx->dmaBufIn.fb);
    close(ctx->dmaBufIn.drmFd);
}

static void initEGL(KMSGrabContext* ctx) {
    eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    eglInitialize(eglDpy, &major, &minor);

    EGLint numConfigs;
    EGLConfig eglCfg;

    eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);
    eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg, pbufferAttribs);
    eglBindAPI(EGL_OPENGL_API);
    eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);
    eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);

    printf("EGL: version %d.%d\n", major, minor);
    printf("EGL: EGL_VERSION: '%s'\n", eglQueryString(eglDpy, EGL_VERSION));
    printf("EGL: EGL_VENDOR: '%s'\n", eglQueryString(eglDpy, EGL_VENDOR));
    printf("EGL: EGL_CLIENT_APIS: '%s'\n", eglQueryString(eglDpy, EGL_CLIENT_APIS));
    printf("EGL: client EGL_EXTENSIONS: '%s'\n", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    printf("EGL: EGL_EXTENSIONS: '%s'\n", eglQueryString(eglDpy, EGL_EXTENSIONS));

    printf("%s\n\n", glGetString(GL_EXTENSIONS));

    PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA =
        (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)eglGetProcAddress("eglExportDMABUFImageQueryMESA");
    PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA =
        (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    // FIXME check for EGL_EXT_image_dma_buf_import
    EGLAttrib inImg_attrs[] = {
        EGL_WIDTH, ctx->dmaBufIn.width,
        EGL_HEIGHT, ctx->dmaBufIn.height,
        EGL_LINUX_DRM_FOURCC_EXT, ctx->dmaBufIn.fourcc,
        EGL_DMA_BUF_PLANE0_FD_EXT, ctx->dmaBufIn.fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, ctx->dmaBufIn.offset,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, ctx->dmaBufIn.pitch,
        EGL_NONE
    };
    eglInImg = eglCreateImage(eglDpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, inImg_attrs);
    ASSERT(eglInImg);

    // FIXME check for GL_OES_EGL_image (or alternatives)
    glGenTextures(1, &texIn);
    printf("generated texture texIn %d\n", texIn);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texIn);
    ASSERT(glEGLImageTargetTexture2DOES);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglInImg);
    ASSERT(glGetError() == 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    //setup render to texture
//    glGenFramebuffers(1, &framebufferName);
//    glBindFramebuffer(GL_FRAMEBUFFER, framebufferName);
//
//    glGenTextures(1, &texOut);
//    printf("generated texture texOut %d\n", texOut);
//    glActiveTexture(GL_TEXTURE1);
//    glBindTexture(GL_TEXTURE_2D, texOut);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
//                 ctx->width, ctx->height, 0, GL_RGBA,
//                 GL_UNSIGNED_BYTE, NULL);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texOut, 0);
//    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
//    glDrawBuffers(1, drawBuffers);
//
//    GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//    if (status != GL_FRAMEBUFFER_COMPLETE) {
//        printf("glCheckFramebufferStatus: %d\n", status);
//    }
//
//    glClearColor(1, 1, 0, 1);
//    glClear(GL_COLOR_BUFFER_BIT);

    //export texture as dmabuf
    EGLAttrib outImg_attrs[] = {
        EGL_WIDTH, ctx->dmaBufOut.width,
        EGL_HEIGHT, ctx->dmaBufOut.height,
        EGL_LINUX_DRM_FOURCC_EXT, ctx->dmaBufOut.fourcc,
        EGL_NONE
    };

    //export framebuffer as dmaBuf
    GLuint renderBuffer;
    glGenRenderbuffers(1, &renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES , ctx->width, ctx->height);
//    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA , ctx->width, ctx->height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderBuffer);

    GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(result != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "framebuffer incomplete! %d\n", result);
    }

    printf("createImage\n");
    eglOutImg = eglCreateImage(eglDpy, eglCtx, EGL_GL_RENDERBUFFER, (EGLClientBuffer)(uint64_t)renderBuffer, NULL);

//    eglExportDMABUFImageQueryMESA()

    int fdOut = -1;
    EGLint stride;
    EGLint offset;
    eglExportDMABUFImageMESA(eglDpy, eglOutImg, &fdOut, &stride, &offset, );

    printf("file descriptor: %d\n", fdOut);
    ctx->dmaBufOut.fd = fdOut;

    const char *fragment =
        "#version 120\n"
        "uniform vec2 res;\n"
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "vec2 uv = gl_FragCoord.xy / res;\n"
        "uv.y = 1. - uv.y;\n"
//        "gl_FragColor = vec4(1.0,0.0,0.0,1.0);\n" blue
//        "gl_FragColor = vec4(0.0,0.0,1.0,1.0);\n" //red
        "gl_FragColor = texture2D(tex, uv);\n"
        "}\n"
    ;

//    GLuint  shader = glCreateShader(GL_FRAGMENT_SHADER);
//    glShaderSource  ( shader , 1 , &fragment , NULL );
//    glCompileShader ( shader );
//    eglProg  = glCreateProgram ();                 // create program object
//    glAttachShader ( eglProg, shader );             // and attach both...

    eglProg = ((PFNGLCREATESHADERPROGRAMVPROC)(eglGetProcAddress("glCreateShaderProgramv")))(GL_FRAGMENT_SHADER, 1, &fragment);

    GLchar log[10000];
    GLint maxLength = 10000;
    glGetProgramInfoLog(eglProg, maxLength, &maxLength, log);

    fprintf(stderr, "%s\n", log);

    glUseProgram(eglProg);
    glUniform1i(glGetUniformLocation(eglProg, "tex"), 0);
    glVertexAttribPointer(
        0, //vertexPosition_modelspaceID, // The attribute we want to configure
        3,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        fullrect_vertex_buffer_data // (void*)0            // array buffer offset
    );
    glEnableVertexAttribArray ( 0 );
}

static void killEGL(KMSGrabContext *ctx) {
    eglMakeCurrent(eglDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDpy, eglCtx);
    eglDestroySurface(eglDpy, eglSurf);
    eglTerminate(eglDpy);
}

KMSGrabContext* initKmsGrab() {
//    0: 0xcd
//    width=720 height=480 pitch=2944 bpp=32 depth=24 handle=0
    kmsGrabContext.width = 1920;
    kmsGrabContext.height = 1080;
    kmsGrabContext.fb_id = 0xcf; //full
//    kmsGrabContext.fb_id = 0x5f; //fake
    strcpy(kmsGrabContext.card,"/dev/dri/card0");

    initDmaBuf(&kmsGrabContext);
    initEGL(&kmsGrabContext);
    return &kmsGrabContext;
}


static int count = 0;
void runKmsGrab(KMSGrabContext *ctx) {
    glBindFramebuffer( GL_FRAMEBUFFER, frameBuffer );
    GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, DrawBuffers);

    glViewport(0, 0, ctx->width, ctx->height);

    glClearColor(1.0, 1.0, 1.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(eglProg);
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(glGetUniformLocation(eglProg, "res"), ctx->width, ctx->height);
    glUniform1i(glGetUniformLocation(eglProg, "tex"), 0);
    glVertexAttribPointer(
        0, //vertexPosition_modelspaceID, // The attribute we want to configure
        3,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        fullrect_vertex_buffer_data // (void*)0            // array buffer offset
    );
    glEnableVertexAttribArray ( 0 );
    glDrawArrays(GL_TRIANGLES, 0, 6);

//    ASSERT(eglSwapBuffers(eglDpy, eglSurf));
}

void killKmsGrab(KMSGrabContext *ctx) {
    killEGL(ctx);
    killDmaBuf(ctx);
}

//sleep(1000);
//memcpy(buffer, tformatbuffer, 1920*1080*4);
//char fname[255];
//sprintf(fname, "dump%d.raw", frame++);
//FILE *fid = fopen(fname, "w+");
//fwrite(buffer, 1, 1920*1080*4, fid);
//fclose(fid);
