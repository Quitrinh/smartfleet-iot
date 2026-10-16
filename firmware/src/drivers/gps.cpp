#include <Arduino.h>
#include <TinyGPSPlus.h>

#include "drivers/gps.h"
#include "config/pins.h"
#include "config/settings.h"
#include "core/globals.h"

// ============================================================
// GPS
// ============================================================

static TinyGPSPlus gps;

// GPS sử dụng UART1
static HardwareSerial gpsSerial(1);


// ============================================================
// INIT GPS
// ============================================================

bool initGPS()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" GPS NEO-7M");
    Serial.println("================================");

    Serial.print("RX GPIO : ");
    Serial.println(GPS_RX_PIN);

    Serial.print("TX GPIO : ");
    Serial.println(GPS_TX_PIN);

    Serial.print("Baud    : ");
    Serial.println(GPS_BAUD);


    // ========================================================
    // UART1
    // begin(baud, config, RX, TX)
    // ========================================================

    gpsSerial.begin(
        GPS_BAUD,
        SERIAL_8N1,
        GPS_RX_PIN,
        GPS_TX_PIN
    );


    gpsValid = false;
    gpsSatellites = 0;


    Serial.println("[OK] GPS UART initialized");

    return true;
}


// ============================================================
// READ GPS
// ============================================================

bool readGPS(uint32_t timeoutMs)
{
    unsigned long start = millis();

    gpsValid = false;


    Serial.println();
    Serial.println("[GPS] Searching location...");


    // ========================================================
    // ĐỌC GPS
    // ========================================================

    while (millis() - start < timeoutMs)
    {
        while (gpsSerial.available() > 0)
        {
            char c = gpsSerial.read();

            // Đưa từng byte NMEA vào TinyGPS++
            gps.encode(c);
        }


        // ====================================================
        // SATELLITES
        // ====================================================

        if (gps.satellites.isValid())
        {
            gpsSatellites =
                gps.satellites.value();
        }


        // ====================================================
        // LOCATION
        // ====================================================

        if (
            gps.location.isValid() &&
            gps.location.age() < 5000
        )
        {
            gpsLat =
                gps.location.lat();

            gpsLng =
                gps.location.lng();

            gpsValid = true;


            Serial.println();
            Serial.println(
                "========== GPS OK =========="
            );

            Serial.print("LAT        : ");
            Serial.println(
                gpsLat,
                6
            );

            Serial.print("LNG        : ");
            Serial.println(
                gpsLng,
                6
            );

            Serial.print("SATELLITES : ");
            Serial.println(
                gpsSatellites
            );

            Serial.print("UART CHARS : ");
            Serial.println(
                gps.charsProcessed()
            );

            Serial.println(
                "============================"
            );


            return true;
        }


        vTaskDelay(
            pdMS_TO_TICKS(20)
        );
    }


    // ========================================================
    // TIMEOUT
    // ========================================================

    gpsValid = false;


    Serial.println();
    Serial.println("[GPS] NO FIX");


    Serial.print("[GPS] SATELLITES: ");
    Serial.println(
        gpsSatellites
    );


    Serial.print("[GPS] UART chars: ");
    Serial.println(
        gps.charsProcessed()
    );


    // ========================================================
    // CHẨN ĐOÁN
    // ========================================================

    if (gps.charsProcessed() == 0)
    {
        Serial.println(
            "[GPS ERROR] NO UART DATA"
        );

        Serial.println(
            "[GPS CHECK] NEO-7M TX -> ESP32 GPIO41"
        );

        Serial.println(
            "[GPS CHECK] Baud = 9600"
        );
    }
    else if (gpsSatellites == 0)
    {
        Serial.println(
            "[GPS] UART OK but no satellite fix"
        );
    }


    return false;
}


// ============================================================
// GET UART CHARACTER COUNT
// ============================================================

uint32_t getGPSCharactersProcessed()
{
    return gps.charsProcessed();
}