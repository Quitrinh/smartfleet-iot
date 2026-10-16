// ============================================================
// task_gps.cpp
//
// EVENT DRIVEN GPS VERSION
//
// GPS chỉ lấy vị trí khi:
// 1. Có BIT_NEED_GPS
// hoặc
// 2. BIT_TRACKING_MODE đang bật và đủ thời gian tracking
// ============================================================

#include <Arduino.h>

#include "tasks/task_gps.h"

#include "drivers/gps.h"

#include "core/globals.h"
#include "core/system_bits.h"

#include "config/settings.h"


// ============================================================
// GPS TASK
// ============================================================

void taskGPS(void *pv)
{
    Serial.println();
    Serial.println("[TASK GPS] STARTED");


    // ========================================================
    // INITIALIZE GPS
    // ========================================================

    if (!initGPS())
    {
        Serial.println(
            "[TASK GPS] GPS INIT FAILED"
        );

        vTaskDelete(NULL);
        return;
    }


    // Thời điểm GPS lấy vị trí thành công gần nhất
    unsigned long lastSendGPS = 0;


    // ========================================================
    // TASK LOOP
    // ========================================================

    while (1)
    {
        // ====================================================
        // READ SYSTEM EVENTS
        // ====================================================

        EventBits_t bits =
            xEventGroupGetBits(
                systemEvents
            );


        // ====================================================
        // MANUAL GPS REQUEST
        // ====================================================

        bool manualRequest =
            (bits & BIT_NEED_GPS) != 0;


        // ====================================================
        // TRACKING GPS REQUEST
        // ====================================================

        bool trackingRequest =
            ((bits & BIT_TRACKING_MODE) != 0) &&
            (
                millis() - lastSendGPS >=
                GPS_TRACKING_INTERVAL_MS
            );


        // ====================================================
        // CHECK IF GPS IS REQUIRED
        // ====================================================

        bool needGPS =
            manualRequest ||
            trackingRequest;


        // ====================================================
        // GPS NOT REQUIRED
        // ====================================================

        if (!needGPS)
        {
            vTaskDelay(
                pdMS_TO_TICKS(500)
            );

            continue;
        }


        // ====================================================
        // CLEAR OLD REQUEST / OLD READY FLAG
        // ====================================================

        xEventGroupClearBits(
            systemEvents,
            BIT_NEED_GPS |
            BIT_GPS_READY
        );


        Serial.println();
        Serial.println(
            "[GPS TASK] GET LOCATION"
        );


        // ====================================================
        // READ GPS
        // ====================================================

        bool ok =
            readGPS(
                GPS_FIX_TIMEOUT_MS
            );


        // ====================================================
        // GPS SUCCESS
        // ====================================================

        if (ok)
        {
            lastSendGPS =
                millis();


            Serial.println(
                "[GPS TASK] LOCATION READY"
            );


            // GPS data đã sẵn sàng
            xEventGroupSetBits(
                systemEvents,
                BIT_GPS_READY
            );


            // =================================================
            // SAU NÀY KHI A7680C HOẠT ĐỘNG:
            //
            // xEventGroupSetBits(
            //     systemEvents,
            //     BIT_GPS_READY |
            //     BIT_NEED_SIM
            // );
            //
            // Hiện tại CHƯA bật BIT_NEED_SIM.
            // =================================================
        }


        // ====================================================
        // GPS FAILED
        // ====================================================

        else
        {
            gpsValid = false;


            Serial.println(
                "[GPS TASK] LOCATION FAILED"
            );
        }


        // ====================================================
        // GIVE CPU TO OTHER TASKS
        // ====================================================

        vTaskDelay(
            pdMS_TO_TICKS(500)
        );
    }
}


// ============================================================
// CREATE GPS TASK
// ============================================================

bool createTaskGPS()
{
    BaseType_t result =
        xTaskCreate(
            taskGPS,       // Function
            "taskGPS",     // Task name
            4096,          // Stack size
            NULL,          // Parameter
            2,             // Priority
            NULL           // Task handle
        );


    // ========================================================
    // CHECK TASK CREATION
    // ========================================================

    if (result != pdPASS)
    {
        Serial.println(
            "[ERROR] Cannot create GPS task"
        );

        return false;
    }


    Serial.println(
        "[OK] GPS task created"
    );


    return true;
}