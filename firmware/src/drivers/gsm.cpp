#include <Arduino.h>

#include "drivers/gsm.h"
#include "config/pins.h"
#include "core/globals.h"

// ============================================================
// A7680C UART
// ============================================================

static HardwareSerial gsmSerial(2);

#define GSM_BAUD 115200


// ============================================================
// DANH SÁCH SỐ ĐIỆN THOẠI NHẬN SMS
// ============================================================
//
// Ví dụ:
// 0912345678  -> +84912345678
//
// Không cần gửi số thật cho mình.
// Tự thay trực tiếp các số bên dưới.
//
// Muốn thêm số mới chỉ cần thêm một dòng:
// "+84xxxxxxxxx",
//
// ============================================================

static const char *smsPhones[] =
{
    "+84337027822",     // Số điện thoại 1
    "+84986831371",     // Số điện thoại 2
};


// Tự động tính số lượng số điện thoại
static const int smsPhoneCount =
    sizeof(smsPhones) / sizeof(smsPhones[0]);


// ============================================================
// CLEAR UART BUFFER
// ============================================================

static void clearGSMBuffer()
{
    while (gsmSerial.available() > 0)
    {
        gsmSerial.read();
    }
}


// ============================================================
// SEND AT COMMAND
// ============================================================

bool sendATCommand(
    const char *command,
    const char *expected,
    uint32_t timeoutMs
)
{
    clearGSMBuffer();

    Serial.println();
    Serial.println("--------------------------------");

    Serial.print("[GSM TX] ");
    Serial.println(command);


    // Gửi lệnh AT
    gsmSerial.println(command);


    String response = "";

    unsigned long start =
        millis();


    // ========================================================
    // WAIT RESPONSE
    // ========================================================

    while (millis() - start < timeoutMs)
    {
        while (gsmSerial.available() > 0)
        {
            char c =
                gsmSerial.read();


            if (c == '\0')
            {
                continue;
            }


            // In phản hồi ra Serial Monitor
            Serial.write(c);


            // Lưu response
            response += c;


            // Kiểm tra chuỗi mong muốn
            if (response.indexOf(expected) >= 0)
            {
                Serial.println();
                Serial.println("[GSM] OK");

                return true;
            }


            // Modem báo lỗi
            if (response.indexOf("ERROR") >= 0)
            {
                Serial.println();
                Serial.println("[GSM] ERROR");

                return false;
            }
        }


        vTaskDelay(
            pdMS_TO_TICKS(10)
        );
    }


    Serial.println();
    Serial.println("[GSM] TIMEOUT");


    return false;
}


// ============================================================
// INIT GSM
// ============================================================

bool initGSM()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" A7680C INIT");
    Serial.println("================================");

    Serial.print("RX GPIO : ");
    Serial.println(GSM_RX_PIN);

    Serial.print("TX GPIO : ");
    Serial.println(GSM_TX_PIN);

    Serial.print("BAUD    : ");
    Serial.println(GSM_BAUD);


    // ========================================================
    // UART2
    // ========================================================

    gsmSerial.begin(
        GSM_BAUD,
        SERIAL_8N1,
        GSM_RX_PIN,
        GSM_TX_PIN
    );


    // Chờ modem ổn định
    delay(5000);


    // ========================================================
    // THỬ AT TỐI ĐA 10 LẦN
    // ========================================================

    for (int i = 1; i <= 10; i++)
    {
        Serial.print("[GSM] AT attempt ");
        Serial.print(i);
        Serial.println("/10");


        if (
            sendATCommand(
                "AT",
                "OK",
                2000
            )
        )
        {
            Serial.println();
            Serial.println(
                "[GSM] MODEM READY"
            );


            // Tắt echo
            sendATCommand(
                "ATE0",
                "OK",
                2000
            );


            // =================================================
            // SMS TEXT MODE
            // =================================================

            sendATCommand(
                "AT+CMGF=1",
                "OK",
                2000
            );


            // =================================================
            // CHARACTER SET
            // =================================================

            sendATCommand(
                "AT+CSCS=\"GSM\"",
                "OK",
                2000
            );


            return true;
        }


        vTaskDelay(
            pdMS_TO_TICKS(1000)
        );
    }


    Serial.println();
    Serial.println(
        "[GSM] MODEM NOT RESPONDING"
    );


    return false;
}


// ============================================================
// TEST GSM
// ============================================================

bool testGSM()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" A7680C STATUS TEST");
    Serial.println("================================");


    // ========================================================
    // SIM
    // ========================================================

    if (
        !sendATCommand(
            "AT+CPIN?",
            "READY",
            3000
        )
    )
    {
        Serial.println(
            "[GSM] SIM NOT READY"
        );

        return false;
    }


    // ========================================================
    // SIGNAL
    // ========================================================

    if (
        !sendATCommand(
            "AT+CSQ",
            "+CSQ:",
            3000
        )
    )
    {
        Serial.println(
            "[GSM] CSQ FAILED"
        );

        return false;
    }


    // ========================================================
    // LTE REGISTRATION
    // ========================================================

    if (
        !sendATCommand(
            "AT+CEREG?",
            "+CEREG:",
            3000
        )
    )
    {
        Serial.println(
            "[GSM] CEREG FAILED"
        );

        return false;
    }


    Serial.println();
    Serial.println("================================");
    Serial.println(" GSM TEST PASS");
    Serial.println("================================");


    return true;
}


// ============================================================
// SEND SMS TO ONE PHONE
// ============================================================

bool sendSMS(
    const char *phone,
    const String &message
)
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" SEND SMS");
    Serial.println("================================");

    Serial.print("[SMS] TO: ");
    Serial.println(phone);


    // ========================================================
    // CHECK MODEM
    // ========================================================

    if (
        !sendATCommand(
            "AT",
            "OK",
            2000
        )
    )
    {
        Serial.println(
            "[SMS] MODEM NOT READY"
        );

        return false;
    }


    // ========================================================
    // SMS TEXT MODE
    // ========================================================

    if (
        !sendATCommand(
            "AT+CMGF=1",
            "OK",
            2000
        )
    )
    {
        Serial.println(
            "[SMS] TEXT MODE FAILED"
        );

        return false;
    }


    // ========================================================
    // CLEAR BUFFER
    // ========================================================

    clearGSMBuffer();


    // ========================================================
    // AT+CMGS
    // ========================================================

    Serial.print("[SMS] AT+CMGS -> ");
    Serial.println(phone);


    gsmSerial.print(
        "AT+CMGS=\""
    );

    gsmSerial.print(
        phone
    );

    gsmSerial.println(
        "\""
    );


    // ========================================================
    // WAIT FOR >
    // ========================================================

    String response = "";

    unsigned long start =
        millis();


    bool promptReady = false;


    while (millis() - start < 5000)
    {
        while (gsmSerial.available() > 0)
        {
            char c =
                gsmSerial.read();


            Serial.write(c);

            response += c;


            if (c == '>')
            {
                promptReady = true;
                break;
            }


            if (
                response.indexOf("ERROR") >= 0
            )
            {
                Serial.println();
                Serial.println(
                    "[SMS] CMGS ERROR"
                );

                return false;
            }
        }


        if (promptReady)
        {
            break;
        }


        vTaskDelay(
            pdMS_TO_TICKS(10)
        );
    }


    if (!promptReady)
    {
        Serial.println();
        Serial.println(
            "[SMS] NO > PROMPT"
        );

        return false;
    }


    // ========================================================
    // SEND MESSAGE
    // ========================================================

    Serial.println();
    Serial.println(
        "[SMS] Sending message..."
    );


    gsmSerial.print(
        message
    );


    delay(200);


    // Ctrl + Z
    gsmSerial.write(26);


    // ========================================================
    // WAIT FOR +CMGS / OK
    // ========================================================

    response = "";

    start =
        millis();


    while (millis() - start < 15000)
    {
        while (gsmSerial.available() > 0)
        {
            char c =
                gsmSerial.read();


            Serial.write(c);

            response += c;


            if (
                response.indexOf("+CMGS:") >= 0 &&
                response.indexOf("OK") >= 0
            )
            {
                Serial.println();
                Serial.println(
                    "[SMS] SEND SUCCESS"
                );

                return true;
            }


            if (
                response.indexOf("ERROR") >= 0
            )
            {
                Serial.println();
                Serial.println(
                    "[SMS] SEND ERROR"
                );

                return false;
            }
        }


        vTaskDelay(
            pdMS_TO_TICKS(20)
        );
    }


    Serial.println();
    Serial.println(
        "[SMS] SEND TIMEOUT"
    );


    return false;
}


// ============================================================
// CREATE GPS MESSAGE
// ============================================================

String createGPSMessage()
{
    String message = "";


    message += "CANH BAO\n";


    // ========================================================
    // GPS VALID
    // ========================================================

    if (gpsValid)
    {
        message += "Vi tri hien tai:\n";


        message += "Lat: ";

        message += String(
            gpsLat,
            6
        );


        message += "\nLng: ";

        message += String(
            gpsLng,
            6
        );


        message += "\n\nGoogle Maps:\n";


        message +=
            "https://maps.google.com/?q=";


        message += String(
            gpsLat,
            6
        );


        message += ",";


        message += String(
            gpsLng,
            6
        );
    }


    // ========================================================
    // GPS INVALID
    // ========================================================

    else
    {
        message +=
            "Khong lay duoc vi tri GPS.";
    }


    return message;
}


// ============================================================
// SEND GPS SMS TO ONE PHONE
// ============================================================

bool sendGPSSMS(
    const char *phone
)
{
    if (!gpsValid)
    {
        Serial.println();
        Serial.println(
            "[SMS] GPS INVALID"
        );

        return false;
    }


    String message =
        createGPSMessage();


    return sendSMS(
        phone,
        message
    );
}


// ============================================================
// SEND GPS TO ALL PHONES
// ============================================================

bool sendGPSToAllPhones()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" SEND GPS TO ALL PHONES");
    Serial.println("================================");


    // Không gửi tọa độ giả nếu GPS chưa hợp lệ
    if (!gpsValid)
    {
        Serial.println(
            "[SMS] GPS NOT READY"
        );

        return false;
    }


    String message =
        createGPSMessage();


    Serial.println();
    Serial.println("[SMS MESSAGE]");
    Serial.println(message);


    int successCount = 0;


    // ========================================================
    // SEND EACH PHONE
    // ========================================================

    for (
        int i = 0;
        i < smsPhoneCount;
        i++
    )
    {
        Serial.println();
        Serial.println("--------------------------------");

        Serial.print("[SMS] PHONE ");
        Serial.print(i + 1);
        Serial.print("/");
        Serial.println(smsPhoneCount);


        bool ok =
            sendSMS(
                smsPhones[i],
                message
            );


        if (ok)
        {
            successCount++;

            Serial.println(
                "[SMS] PHONE SUCCESS"
            );
        }
        else
        {
            Serial.println(
                "[SMS] PHONE FAILED"
            );
        }


        // Khoảng nghỉ giữa hai SMS
        vTaskDelay(
            pdMS_TO_TICKS(3000)
        );
    }


    // ========================================================
    // RESULT
    // ========================================================

    Serial.println();
    Serial.println("================================");

    Serial.print("[SMS] SUCCESS: ");
    Serial.print(successCount);
    Serial.print("/");
    Serial.println(smsPhoneCount);

    Serial.println("================================");


    return (
        successCount == smsPhoneCount
    );
}