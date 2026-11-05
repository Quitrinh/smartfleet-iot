#ifndef CAMERA_H
#define CAMERA_H

#include <Arduino.h>
#include "esp_camera.h"

// Khởi tạo OV3660
bool initCamera();

// Kiểm tra camera đã sẵn sàng
bool isCameraReady();

// Chụp một frame JPEG.
// Sau khi dùng xong phải gọi releaseCameraFrame().
camera_fb_t *captureCameraFrame();

// Trả framebuffer về camera driver
void releaseCameraFrame(camera_fb_t *frame);

#endif