#include <Arduino.h>
#include <WiFi.h>

#include "esp_camera.h"
#include "esp_http_server.h"

#include "drivers/camera.h"
#include "config/pins.h"


// ============================================================
// HTTP SERVER
// ============================================================

static httpd_handle_t cameraServer = NULL;


// ============================================================
// STREAM HANDLER
// ============================================================

static esp_err_t streamHandler(httpd_req_t *req)
{
    esp_err_t result = httpd_resp_set_type(
        req,
        "multipart/x-mixed-replace;boundary=frame"
    );

    if (result != ESP_OK)
    {
        return result;
    }

    Serial.println("[CAMERA] Stream client connected");

    while (true)
    {
        camera_fb_t *frame = esp_camera_fb_get();

        if (frame == NULL)
        {
            Serial.println("[CAMERA] Capture failed");
            result = ESP_FAIL;
            break;
        }

        char header[128];

        int headerLength = snprintf(
            header,
            sizeof(header),
            "\r\n--frame\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: %u\r\n\r\n",
            frame->len
        );

        result = httpd_resp_send_chunk(
            req,
            header,
            headerLength
        );

        if (result == ESP_OK)
        {
            result = httpd_resp_send_chunk(
                req,
                reinterpret_cast<const char *>(frame->buf),
                frame->len
            );
        }

        esp_camera_fb_return(frame);

        if (result != ESP_OK)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }

    Serial.println("[CAMERA] Stream client disconnected");

    return result;
}


// ============================================================
// HOME PAGE
// ============================================================

static esp_err_t indexHandler(httpd_req_t *req)
{
    const char html[] =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' "
        "content='width=device-width,initial-scale=1'>"
        "<title>ESP32 LOGICTIC</title>"
        "</head>"
        "<body>"
        "<h2>ESP32 LOGICTIC</h2>"
        "<p>Camera Stream</p>"
        "<img src='/stream' style='max-width:100%;height:auto;'>"
        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html");

    return httpd_resp_send(
        req,
        html,
        HTTPD_RESP_USE_STRLEN
    );
}


// ============================================================
// INITIALIZE CAMERA
// ============================================================

bool initCamera()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" CAMERA");
    Serial.println("================================");

    if (psramFound())
    {
        Serial.println("[OK] PSRAM detected");

        Serial.print("PSRAM Size: ");
        Serial.print(ESP.getPsramSize() / 1024 / 1024);
        Serial.println(" MB");
    }
    else
    {
        Serial.println("[WARNING] PSRAM not detected");
    }


    camera_config_t config = {};

    // --------------------------------------------------------
    // Camera GPIO
    // --------------------------------------------------------

    config.pin_pwdn = -1;
    config.pin_reset = -1;

    config.pin_xclk = CAM_PIN_XCLK;

    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;

    config.pin_d7 = CAM_PIN_Y9;
    config.pin_d6 = CAM_PIN_Y8;
    config.pin_d5 = CAM_PIN_Y7;
    config.pin_d4 = CAM_PIN_Y6;
    config.pin_d3 = CAM_PIN_Y5;
    config.pin_d2 = CAM_PIN_Y4;
    config.pin_d1 = CAM_PIN_Y3;
    config.pin_d0 = CAM_PIN_Y2;

    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_pclk = CAM_PIN_PCLK;


    // --------------------------------------------------------
    // Camera configuration
    // --------------------------------------------------------

    config.xclk_freq_hz = 20000000;

    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;

    config.pixel_format = PIXFORMAT_JPEG;

    // 320 x 240
    config.frame_size = FRAMESIZE_QVGA;

    config.jpeg_quality = 12;

    if (psramFound())
    {
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_LATEST;
    }
    else
    {
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }


    // --------------------------------------------------------
    // Initialize
    // --------------------------------------------------------

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK)
    {
        Serial.print("[ERROR] Camera init failed: 0x");
        Serial.println(err, HEX);

        return false;
    }


    Serial.println("[OK] Camera initialized");


    // --------------------------------------------------------
    // Camera sensor
    // --------------------------------------------------------

    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor != NULL)
    {
        Serial.print("Camera PID: 0x");
        Serial.println(sensor->id.PID, HEX);
    }


    // --------------------------------------------------------
    // Test capture
    // --------------------------------------------------------

    camera_fb_t *frame = esp_camera_fb_get();

    if (frame == NULL)
    {
        Serial.println("[ERROR] Camera test capture failed");

        return false;
    }

    Serial.print("Resolution : ");
    Serial.print(frame->width);
    Serial.print("x");
    Serial.println(frame->height);

    Serial.print("JPEG Size  : ");
    Serial.print(frame->len);
    Serial.println(" bytes");

    esp_camera_fb_return(frame);

    return true;
}


// ============================================================
// START CAMERA WEB SERVER
// ============================================================

bool startCameraServer()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" CAMERA WEB SERVER");
    Serial.println("================================");


    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    config.server_port = 80;

    config.max_uri_handlers = 4;


    if (httpd_start(
            &cameraServer,
            &config
        ) != ESP_OK)
    {
        Serial.println(
            "[ERROR] Cannot start camera server"
        );

        return false;
    }


    // --------------------------------------------------------
    // /
    // --------------------------------------------------------

    httpd_uri_t indexURI = {};

    indexURI.uri = "/";
    indexURI.method = HTTP_GET;
    indexURI.handler = indexHandler;
    indexURI.user_ctx = NULL;

    httpd_register_uri_handler(
        cameraServer,
        &indexURI
    );


    // --------------------------------------------------------
    // /stream
    // --------------------------------------------------------

    httpd_uri_t streamURI = {};

    streamURI.uri = "/stream";
    streamURI.method = HTTP_GET;
    streamURI.handler = streamHandler;
    streamURI.user_ctx = NULL;

    httpd_register_uri_handler(
        cameraServer,
        &streamURI
    );


    Serial.println(
        "[OK] Camera server started"
    );

    Serial.print("Web    : http://");
    Serial.println(WiFi.localIP());

    Serial.print("Stream : http://");
    Serial.print(WiFi.localIP());
    Serial.println("/stream");


    return true;
}