#include <Arduino.h>

#include "esp_camera.h"

#include "drivers/camera.h"
#include "config/pins.h"


// ============================================================
// CAMERA STATE
// ============================================================

static bool cameraReady = false;


// ============================================================
// INITIALIZE CAMERA
// ============================================================

bool initCamera()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" CAMERA OV3660");
    Serial.println("================================");


    // ========================================================
    // PSRAM CHECK
    // ========================================================

    bool hasPSRAM = psramFound();

    if (hasPSRAM)
    {
        Serial.println("[OK] PSRAM detected");

        Serial.print("[CAMERA] PSRAM Size: ");
        Serial.print(
            ESP.getPsramSize() / 1024 / 1024
        );
        Serial.println(" MB");
    }
    else
    {
        Serial.println(
            "[WARNING] PSRAM not detected"
        );
    }


    // ========================================================
    // CAMERA CONFIG
    // ========================================================

    camera_config_t config = {};


    // --------------------------------------------------------
    // Power / Reset
    // --------------------------------------------------------

    config.pin_pwdn = -1;
    config.pin_reset = -1;


    // --------------------------------------------------------
    // Clock
    // --------------------------------------------------------

    config.pin_xclk = CAM_PIN_XCLK;

    config.xclk_freq_hz = 20000000;

    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;


    // --------------------------------------------------------
    // SCCB
    // --------------------------------------------------------

    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;


    // --------------------------------------------------------
    // Camera Data Bus
    // --------------------------------------------------------

    config.pin_d7 = CAM_PIN_Y9;
    config.pin_d6 = CAM_PIN_Y8;
    config.pin_d5 = CAM_PIN_Y7;
    config.pin_d4 = CAM_PIN_Y6;

    config.pin_d3 = CAM_PIN_Y5;
    config.pin_d2 = CAM_PIN_Y4;
    config.pin_d1 = CAM_PIN_Y3;
    config.pin_d0 = CAM_PIN_Y2;


    // --------------------------------------------------------
    // Sync
    // --------------------------------------------------------

    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_pclk = CAM_PIN_PCLK;


    // ========================================================
    // IMAGE CONFIGURATION
    // ========================================================

    // JPEG giúp giảm kích thước dữ liệu khi upload qua 4G.
    config.pixel_format = PIXFORMAT_JPEG;


    // --------------------------------------------------------
    // Resolution
    // --------------------------------------------------------
    //
    // Bắt đầu với QVGA 320x240 để kiểm tra pipeline:
    //
    // Camera
    //      ↓
    // ESP32
    //      ↓
    // A7680C
    //      ↓
    // Azure
    //
    // Khi hệ thống ổn định có thể tăng lên VGA.
    //
    // --------------------------------------------------------

    config.frame_size = FRAMESIZE_QVGA;


    // --------------------------------------------------------
    // JPEG quality
    // --------------------------------------------------------
    //
    // ESP camera:
    //
    // số nhỏ hơn = chất lượng cao hơn
    // số lớn hơn = file nhỏ hơn
    //
    // --------------------------------------------------------

    config.jpeg_quality = 12;


    // ========================================================
    // FRAME BUFFER
    // ========================================================

    if (hasPSRAM)
    {
        config.fb_count = 2;

        config.fb_location =
            CAMERA_FB_IN_PSRAM;

        config.grab_mode =
            CAMERA_GRAB_LATEST;
    }
    else
    {
        config.fb_count = 1;

        config.fb_location =
            CAMERA_FB_IN_DRAM;

        config.grab_mode =
            CAMERA_GRAB_WHEN_EMPTY;
    }


    // ========================================================
    // INITIALIZE CAMERA DRIVER
    // ========================================================

    Serial.println(
        "[CAMERA] Initializing..."
    );


    esp_err_t err =
        esp_camera_init(&config);


    if (err != ESP_OK)
    {
        Serial.print(
            "[ERROR] Camera init failed: 0x"
        );

        Serial.println(
            err,
            HEX
        );


        cameraReady = false;

        return false;
    }


    // ========================================================
    // READ SENSOR
    // ========================================================

    sensor_t *sensor =
        esp_camera_sensor_get();


    if (sensor == nullptr)
    {
        Serial.println(
            "[ERROR] Cannot read camera sensor"
        );


        cameraReady = false;

        esp_camera_deinit();

        return false;
    }


    Serial.print(
        "[CAMERA] Sensor PID: 0x"
    );

    Serial.println(
        sensor->id.PID,
        HEX
    );


    // ========================================================
    // OV3660 SETTINGS
    // ========================================================

    // OV3660 PID = 0x3660 on some driver versions,
    // while existing project logs may expose the sensor
    // identifier differently.
    //
    // Không ép cấu hình theo PID ở bước này.
    // Giữ cấu hình mặc định trước để tránh ảnh bị xoay/sai màu.


    // ========================================================
    // TEST CAPTURE
    // ========================================================

    Serial.println(
        "[CAMERA] Test capture..."
    );


    camera_fb_t *frame =
        esp_camera_fb_get();


    if (frame == nullptr)
    {
        Serial.println(
            "[ERROR] Camera test capture failed"
        );


        cameraReady = false;

        esp_camera_deinit();

        return false;
    }


    // ========================================================
    // PRINT TEST INFORMATION
    // ========================================================

    Serial.println();
    Serial.println(
        "--------------------------------"
    );

    Serial.println(
        "[CAMERA] TEST FRAME"
    );


    Serial.print(
        "Resolution : "
    );

    Serial.print(
        frame->width
    );

    Serial.print("x");

    Serial.println(
        frame->height
    );


    Serial.print(
        "JPEG Size  : "
    );

    Serial.print(
        frame->len
    );

    Serial.println(
        " bytes"
    );


    Serial.print(
        "Format     : "
    );

    Serial.println(
        frame->format == PIXFORMAT_JPEG
            ? "JPEG"
            : "OTHER"
    );


    Serial.println(
        "--------------------------------"
    );


    // ========================================================
    // RETURN TEST FRAME
    // ========================================================

    esp_camera_fb_return(frame);


    // ========================================================
    // CAMERA READY
    // ========================================================

    cameraReady = true;


    Serial.println();
    Serial.println(
        "[OK] CAMERA READY"
    );

    Serial.println(
        "================================"
    );


    return true;
}


// ============================================================
// CAMERA READY STATE
// ============================================================

bool isCameraReady()
{
    return cameraReady;
}


// ============================================================
// CAPTURE JPEG FRAME
// ============================================================

camera_fb_t *captureCameraFrame()
{
    // ========================================================
    // CAMERA NOT INITIALIZED
    // ========================================================

    if (!cameraReady)
    {
        Serial.println(
            "[CAMERA] Capture rejected: camera not ready"
        );

        return nullptr;
    }


    // ========================================================
    // GET FRAME
    // ========================================================

    camera_fb_t *frame =
        esp_camera_fb_get();


    if (frame == nullptr)
    {
        Serial.println(
            "[CAMERA] Capture failed"
        );

        return nullptr;
    }


    // ========================================================
    // VERIFY JPEG
    // ========================================================

    if (frame->format != PIXFORMAT_JPEG)
    {
        Serial.println(
            "[CAMERA] Invalid frame format"
        );


        esp_camera_fb_return(frame);

        return nullptr;
    }


    // ========================================================
    // PRINT FRAME INFORMATION
    // ========================================================

    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "[CAMERA] FRAME CAPTURED"
    );


    Serial.print(
        "Resolution : "
    );

    Serial.print(
        frame->width
    );

    Serial.print("x");

    Serial.println(
        frame->height
    );


    Serial.print(
        "JPEG Size  : "
    );

    Serial.print(
        frame->len
    );

    Serial.println(
        " bytes"
    );


    Serial.println(
        "================================"
    );


    // ========================================================
    // IMPORTANT
    // ========================================================
    //
    // KHÔNG esp_camera_fb_return(frame) tại đây.
    //
    // Buffer phải còn tồn tại để gsm.cpp có thể upload:
    //
    // frame->buf
    // frame->len
    //
    // Sau khi upload xong phải gọi:
    //
    // releaseCameraFrame(frame);
    //
    // ========================================================


    return frame;
}


// ============================================================
// RELEASE CAMERA FRAME
// ============================================================

void releaseCameraFrame(
    camera_fb_t *frame
)
{
    if (frame == nullptr)
    {
        return;
    }


    esp_camera_fb_return(frame);


    Serial.println(
        "[CAMERA] Frame released"
    );
}