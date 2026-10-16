#ifndef SYSTEM_BITS_H
#define SYSTEM_BITS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>


// ============================================================
// SYSTEM EVENT BITS
// ============================================================

// Yêu cầu GPS lấy tọa độ
#define BIT_NEED_GPS        (1 << 0)

// GPS đã lấy được tọa độ
#define BIT_GPS_READY       (1 << 1)

// Yêu cầu SIM xử lý
#define BIT_NEED_SIM        (1 << 2)

// Tracking mode
#define BIT_TRACKING_MODE   (1 << 3)

// WiFi đã kết nối
#define BIT_WIFI_READY      (1 << 4)

// Camera đã sẵn sàng
#define BIT_CAMERA_READY    (1 << 5)


// ============================================================
// GLOBAL EVENT GROUP
// ============================================================

extern EventGroupHandle_t systemEvents;


bool initSystemEvents();


#endif