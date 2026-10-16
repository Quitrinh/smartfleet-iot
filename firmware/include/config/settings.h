#ifndef SETTINGS_H
#define SETTINGS_H


// ============================================================
// WIFI
// ============================================================

#define WIFI_SSID       "XXXXXXXXXX"
#define WIFI_PASSWORD   "XXXXXXXXXX"


// ============================================================
// GPS
// ============================================================

// Baud mặc định của NEO-7M
#define GPS_BAUD 9600

// Thời gian tối đa chờ GPS lấy vị trí
#define GPS_FIX_TIMEOUT_MS 8000

// Tracking mode: lấy vị trí mỗi 60 giây
#define GPS_TRACKING_INTERVAL_MS 60000


// ============================================================
// LCD
// ============================================================

#define LCD_I2C_ADDRESS 0x27

#define LCD_COLUMNS 16
#define LCD_ROWS 2


#endif