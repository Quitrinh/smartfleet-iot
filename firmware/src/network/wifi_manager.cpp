#include <Arduino.h>
#include <WiFi.h>

#include "network/wifi_manager.h"

#include "config/settings.h"
#include "core/system_bits.h"


bool initWiFi()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" WIFI");
    Serial.println("================================");


    WiFi.mode(WIFI_STA);

    // Giảm độ trễ camera stream
    WiFi.setSleep(false);


    Serial.print("Connecting to: ");
    Serial.println(WIFI_SSID);


    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );


    // ========================================================
    // WAIT FOR CONNECTION
    // ========================================================

    uint32_t startTime = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");

        delay(500);

        if (millis() - startTime >= 20000)
        {
            Serial.println();
            Serial.println(
                "[ERROR] WiFi connection timeout"
            );

            return false;
        }
    }


    Serial.println();
    Serial.println("[OK] WiFi connected");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");


    // ========================================================
    // FREERTOS EVENT
    // ========================================================

    if (systemEvents != NULL)
    {
        xEventGroupSetBits(
            systemEvents,
            BIT_WIFI_READY
        );
    }


    return true;
}