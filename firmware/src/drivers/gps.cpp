#include <Arduino.h>
#include <TinyGPSPlus.h>

#include "drivers/gps.h"
#include "config/pins.h"
#include "config/settings.h"
#include "core/globals.h"


// ============================================================
// GPS DRIVER - NEO-7M
// ============================================================
//
// NEO-7M
//    ↓ UART1
// ESP32-S3
//    ↓
// TinyGPS++
//    ↓
// Latitude
// Longitude
// Speed
// Satellites
//
// ============================================================


// ============================================================
// TINYGPS++
// ============================================================

static TinyGPSPlus gps;


// ============================================================
// GPS UART
// ============================================================
//
// ESP32-S3 UART1.
//
// begin(
//     baud,
//     config,
//     RX,
//     TX
// )
//
// ============================================================

static HardwareSerial gpsSerial(1);


// ============================================================
// INTERNAL GPS DATA
// ============================================================
//
// Các biến này chỉ được quản lý bên trong gps.cpp.
//
// main.cpp không cần truy cập trực tiếp.
//
// main.cpp sẽ dùng:
//
// getGPSLatitude()
// getGPSLongitude()
// getGPSSpeedKmh()
// getGPSSatellites()
//
// ============================================================

static double currentLatitude = 0.0;

static double currentLongitude = 0.0;

static double currentSpeedKmh = 0.0;

static uint32_t currentSatellites = 0;

static bool currentGPSValid = false;


// ============================================================
// GPS CONFIGURATION
// ============================================================
//
// Không chấp nhận location quá cũ để upload.
//
// 5000 ms = 5 giây.
//
// ============================================================

static const uint32_t GPS_MAX_LOCATION_AGE_MS = 5000;


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
    // START UART1
    // ========================================================

    gpsSerial.begin(
        GPS_BAUD,
        SERIAL_8N1,
        GPS_RX_PIN,
        GPS_TX_PIN
    );


    // ========================================================
    // RESET LOCAL GPS STATE
    // ========================================================

    currentLatitude = 0.0;

    currentLongitude = 0.0;

    currentSpeedKmh = 0.0;

    currentSatellites = 0;

    currentGPSValid = false;


    // ========================================================
    // KEEP GLOBALS COMPATIBLE
    // ========================================================
    //
    // Project cũ của bạn đang dùng:
    //
    // gpsValid
    // gpsLat
    // gpsLng
    // gpsSatellites
    //
    // Vì vậy vẫn cập nhật chúng để các module cũ
    // không bị hỏng.
    //
    // ========================================================

    gpsValid = false;

    gpsLat = 0.0;

    gpsLng = 0.0;

    gpsSatellites = 0;


    Serial.println("[OK] GPS UART initialized");

    return true;
}


// ============================================================
// READ GPS
// ============================================================

bool readGPS(uint32_t timeoutMs)
{
    const uint32_t start = millis();


    // ========================================================
    // RESET VALID FLAG
    // ========================================================

    currentGPSValid = false;

    gpsValid = false;


    Serial.println();
    Serial.println("================================");
    Serial.println(" GPS SEARCH");
    Serial.println("================================");

    Serial.println("[GPS] Searching location...");


    // ========================================================
    // READ UNTIL TIMEOUT
    // ========================================================

    while ((uint32_t)(millis() - start) < timeoutMs)
    {
        // ====================================================
        // READ UART DATA
        // ====================================================

        while (gpsSerial.available() > 0)
        {
            const char c = gpsSerial.read();

            // Feed NMEA byte into TinyGPS++
            gps.encode(c);
        }


        // ====================================================
        // SATELLITES
        // ====================================================

        if (gps.satellites.isValid())
        {
            currentSatellites =
                (uint32_t)gps.satellites.value();

            // Keep old global synchronized.
            gpsSatellites = currentSatellites;
        }


        // ====================================================
        // SPEED
        // ====================================================

        if (gps.speed.isValid())
        {
            currentSpeedKmh =
                gps.speed.kmph();

            // Avoid impossible negative values.
            if (currentSpeedKmh < 0.0)
            {
                currentSpeedKmh = 0.0;
            }
        }
        else
        {
            currentSpeedKmh = 0.0;
        }


        // ====================================================
        // LOCATION
        // ====================================================

        if (
            gps.location.isValid() &&
            gps.location.age() <= GPS_MAX_LOCATION_AGE_MS
        )
        {
            const double latitude =
                gps.location.lat();

            const double longitude =
                gps.location.lng();


            // =================================================
            // BASIC RANGE VALIDATION
            // =================================================

            if (
                latitude >= -90.0 &&
                latitude <= 90.0 &&
                longitude >= -180.0 &&
                longitude <= 180.0
            )
            {
                currentLatitude = latitude;

                currentLongitude = longitude;

                currentGPSValid = true;


                // =============================================
                // KEEP OLD GLOBALS SYNCHRONIZED
                // =============================================

                gpsLat = currentLatitude;

                gpsLng = currentLongitude;

                gpsValid = true;

                gpsSatellites = currentSatellites;


                // =============================================
                // DEBUG
                // =============================================

                Serial.println();
                Serial.println(
                    "========== GPS FIX =========="
                );

                Serial.print("LAT        : ");
                Serial.println(
                    currentLatitude,
                    6
                );

                Serial.print("LNG        : ");
                Serial.println(
                    currentLongitude,
                    6
                );

                Serial.print("SPEED KM/H : ");
                Serial.println(
                    currentSpeedKmh,
                    2
                );

                Serial.print("SATELLITES : ");
                Serial.println(
                    currentSatellites
                );

                Serial.print("AGE MS     : ");
                Serial.println(
                    gps.location.age()
                );

                Serial.print("UART CHARS : ");
                Serial.println(
                    gps.charsProcessed()
                );

                Serial.println(
                    "============================="
                );


                return true;
            }
        }


        // ====================================================
        // SMALL DELAY
        // ====================================================

        vTaskDelay(
            pdMS_TO_TICKS(20)
        );
    }


    // ========================================================
    // TIMEOUT
    // ========================================================

    currentGPSValid = false;

    gpsValid = false;


    Serial.println();
    Serial.println("================================");
    Serial.println(" GPS NO FIX");
    Serial.println("================================");


    Serial.print("[GPS] Satellites : ");
    Serial.println(
        currentSatellites
    );


    Serial.print("[GPS] UART chars : ");
    Serial.println(
        gps.charsProcessed()
    );


    // ========================================================
    // LOCATION STATUS
    // ========================================================

    Serial.print("[GPS] Location valid : ");

    Serial.println(
        gps.location.isValid()
            ? "YES"
            : "NO"
    );


    if (gps.location.isValid())
    {
        Serial.print("[GPS] Location age   : ");

        Serial.print(
            gps.location.age()
        );

        Serial.println(" ms");
    }


    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    if (gps.charsProcessed() == 0)
    {
        Serial.println();
        Serial.println(
            "[GPS ERROR] NO UART DATA"
        );

        Serial.println(
            "[GPS CHECK] Check NEO-7M power"
        );

        Serial.print(
            "[GPS CHECK] GPS TX -> ESP32 GPIO"
        );

        Serial.println(
            GPS_RX_PIN
        );

        Serial.print(
            "[GPS CHECK] Baud = "
        );

        Serial.println(
            GPS_BAUD
        );
    }
    else if (currentSatellites == 0)
    {
        Serial.println();

        Serial.println(
            "[GPS] UART data received."
        );

        Serial.println(
            "[GPS] No satellite fix yet."
        );
    }
    else
    {
        Serial.println();

        Serial.println(
            "[GPS] NMEA data received but "
            "location is not ready."
        );
    }


    return false;
}


// ============================================================
// GPS VALID
// ============================================================

bool isGPSValid()
{
    return currentGPSValid;
}


// ============================================================
// GET LATITUDE
// ============================================================

double getGPSLatitude()
{
    return currentLatitude;
}


// ============================================================
// GET LONGITUDE
// ============================================================

double getGPSLongitude()
{
    return currentLongitude;
}


// ============================================================
// GET SPEED
// ============================================================

double getGPSSpeedKmh()
{
    if (currentSpeedKmh < 0.0)
    {
        return 0.0;
    }

    return currentSpeedKmh;
}


// ============================================================
// GET SATELLITES
// ============================================================

uint32_t getGPSSatellites()
{
    return currentSatellites;
}


// ============================================================
// GET UART CHARACTER COUNT
// ============================================================

uint32_t getGPSCharactersProcessed()
{
    return (uint32_t)gps.charsProcessed();
}


// ============================================================
// GET LOCATION AGE
// ============================================================

uint32_t getGPSLocationAge()
{
    if (!gps.location.isValid())
    {
        return UINT32_MAX;
    }

    return (uint32_t)gps.location.age();
}


// ============================================================
// GPS READY FOR CLOUD UPLOAD
// ============================================================

bool isGPSReadyForUpload()
{
    // ========================================================
    // GPS FIX MUST BE VALID
    // ========================================================

    if (!currentGPSValid)
    {
        return false;
    }


    // ========================================================
    // TINYGPS++ LOCATION MUST BE VALID
    // ========================================================

    if (!gps.location.isValid())
    {
        return false;
    }


    // ========================================================
    // LOCATION MUST NOT BE OLD
    // ========================================================

    if (
        gps.location.age() >
        GPS_MAX_LOCATION_AGE_MS
    )
    {
        return false;
    }


    // ========================================================
    // LATITUDE RANGE
    // ========================================================

    if (
        currentLatitude < -90.0 ||
        currentLatitude > 90.0
    )
    {
        return false;
    }


    // ========================================================
    // LONGITUDE RANGE
    // ========================================================

    if (
        currentLongitude < -180.0 ||
        currentLongitude > 180.0
    )
    {
        return false;
    }


    // ========================================================
    // DATA READY
    // ========================================================

    return true;
}