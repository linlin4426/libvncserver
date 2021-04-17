#ifndef MMALTEST_H264_ENCODER_H
#define MMALTEST_H264_ENCODER_H

#include "interface/mmal/mmal.h"

typedef int (*onFrameCb)(MMAL_BUFFER_HEADER_T*);

int mmalh264_encoder_init(int frame_width, int frame_height);
int mmalh264_encoder_encode(u_char* frame_buffer, int width, int height, onFrameCb on_frame_cb);
int mmalh264_encoder_stop();


#endif //MMALTEST_JPEG_ENCODER_H