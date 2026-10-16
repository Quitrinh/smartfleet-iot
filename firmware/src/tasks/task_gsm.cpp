#include <Arduino.h>

#include "tasks/task_gsm.h"
#include "drivers/gsm.h"
#include "core/system_bits.h"

void taskGSM(void *pv)
{
    Serial.println();
    Serial.println("[TASK GSM] STARTED");

    // ==========================================
    // KHỞI TẠO A7680C
    // ==========================================

    if (!initGSM())
    {
        Serial.println("[TASK GSM] INIT FAILED");

        vTaskDelete(NULL);
        return;
    }

    Serial.println("[TASK GSM] MODEM INITIALIZED");


    // ==========================================
    // KIỂM TRA SIM + SIGNAL + LTE
    // ==========================================

    if (!testGSM())
    {
        Serial.println("[TASK GSM] STATUS TEST FAILED");
    }
    else
    {
        Serial.println("[TASK GSM] STATUS TEST PASS");
    }


    // ==========================================
    // TASK LOOP
    // ==========================================

    while (1)
    {
        EventBits_t bits =
            xEventGroupGetBits(systemEvents);


        // Chưa có yêu cầu sử dụng SIM
        if ((bits & BIT_NEED_SIM) == 0)
        {
            vTaskDelay(
                pdMS_TO_TICKS(500)
            );

            continue;
        }


        // ======================================
        // CÓ YÊU CẦU GSM
        // ======================================

        xEventGroupClearBits(
            systemEvents,
            BIT_NEED_SIM
        );


        Serial.println();
        Serial.println("==============================");
        Serial.println("[GSM TASK] REQUEST RECEIVED");
        Serial.println("==============================");


        // Hiện tại chỉ test modem
        // Chưa gửi SMS

        bool ok = testGSM();


        if (ok)
        {
            Serial.println(
                "[GSM TASK] MODEM READY"
            );
        }
        else
        {
            Serial.println(
                "[GSM TASK] MODEM ERROR"
            );
        }


        vTaskDelay(
            pdMS_TO_TICKS(500)
        );
    }
}


// ============================================================
// CREATE TASK
// ============================================================

bool createTaskGSM()
{
    BaseType_t result =
        xTaskCreate(
            taskGSM,
            "taskGSM",
            4096,
            NULL,
            2,
            NULL
        );


    if (result != pdPASS)
    {
        Serial.println(
            "[ERROR] Cannot create GSM task"
        );

        return false;
    }


    Serial.println(
        "[OK] GSM task created"
    );

    return true;
}