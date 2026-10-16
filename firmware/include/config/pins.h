#ifndef PINS_H
#define PINS_H

// ============================================================
// LCD I2C
// ============================================================

#define I2C_SDA_PIN 1
#define I2C_SCL_PIN 2


// ============================================================
// GPS NEO-7M - UART1
// ============================================================

#define GPS_RX_PIN 41
#define GPS_TX_PIN 42


// ============================================================
// A7680C - UART2
// ============================================================

#define GSM_RX_PIN 21
#define GSM_TX_PIN 47


// ============================================================
// CAMERA OV3660
// ============================================================

#define CAM_PIN_SIOD   4
#define CAM_PIN_SIOC   5

#define CAM_PIN_VSYNC  6
#define CAM_PIN_HREF   7

#define CAM_PIN_Y4     8
#define CAM_PIN_Y3     9
#define CAM_PIN_Y5     10
#define CAM_PIN_Y2     11
#define CAM_PIN_Y6     12

#define CAM_PIN_PCLK   13
#define CAM_PIN_XCLK   15

#define CAM_PIN_Y9     16
#define CAM_PIN_Y8     17
#define CAM_PIN_Y7     18


// ============================================================
// RGB LED
// ============================================================

#define RGB_LED_PIN 48


// ============================================================
// RFID RC522
// ============================================================

#define RFID_SS_PIN    3
#define RFID_RST_PIN   14

#define RFID_SCK_PIN   40
#define RFID_MOSI_PIN  38
#define RFID_MISO_PIN  39

#endif