// #include <Arduino.h>

// // ============================================================
// // CONFIG
// // ============================================================

// #include "config/pins.h"
// #include "config/settings.h"


// // ============================================================
// // CORE
// // ============================================================

// #include "core/system_bits.h"
// #include "core/globals.h"


// // ============================================================
// // DRIVERS
// // ============================================================

// #include "drivers/camera.h"
// #include "drivers/lcd.h"
// #include "drivers/rfid.h"


// // ============================================================
// // NETWORK
// // ============================================================

// #include "network/wifi_manager.h"


// // ============================================================
// // TASKS
// // ============================================================

// #include "tasks/task_gps.h"
// #include "tasks/task_gsm.h"


// // ============================================================
// // SYSTEM STATE
// // ============================================================

// // Dùng để tránh xử lý cùng một GPS_READY nhiều lần
// static bool gpsDisplayed = false;


// // ============================================================
// // RFID CONFIG
// // ============================================================

// // UID thẻ được phép
// static const String AUTHORIZED_UID = "6B C8 99 8A";

// // RFID có hoạt động hay không
// static bool rfidReady = false;

// // Thời điểm RFID hiển thị thông báo gần nhất
// static unsigned long rfidDisplayStart = 0;

// // RFID có đang chiếm LCD để hiển thị kết quả không
// static bool rfidShowingMessage = false;

// // Thời gian giữ thông báo RFID trên LCD
// static const unsigned long RFID_MESSAGE_TIME = 2000;


// // ============================================================
// // SHOW GPS ON LCD
// // ============================================================

// void showGPSOnLCD()
// {
//     // Nếu GPS chưa hợp lệ
//     if (!gpsValid)
//     {
//         lcdShowMessage(
//             "GPS ERROR",
//             "NO LOCATION"
//         );

//         return;
//     }


//     // LCD 16x2
//     char line1[17];
//     char line2[17];


//     // Latitude
//     snprintf(
//         line1,
//         sizeof(line1),
//         "LAT:%.5f",
//         gpsLat
//     );


//     // Longitude
//     snprintf(
//         line2,
//         sizeof(line2),
//         "LNG:%.5f",
//         gpsLng
//     );


//     lcdShowMessage(
//         line1,
//         line2
//     );
// }


// // ============================================================
// // HANDLE RFID
// // ============================================================

// void handleRFID()
// {
//     // Nếu RFID init lỗi thì bỏ qua
//     if (!rfidReady)
//     {
//         return;
//     }


//     String uid;


//     // Không có thẻ mới
//     if (!readRFID(uid))
//     {
//         return;
//     }


//     // ========================================================
//     // CARD DETECTED
//     // ========================================================

//     Serial.println();
//     Serial.println("========================================");
//     Serial.println("[RFID] CARD DETECTED");
//     Serial.println("========================================");

//     Serial.print("[RFID] UID : ");
//     Serial.println(uid);


//     // ========================================================
//     // AUTHORIZED CARD
//     // ========================================================

//     if (uid == AUTHORIZED_UID)
//     {
//         Serial.println(
//             "[RFID] ACCESS GRANTED"
//         );


//         lcdShowMessage(
//             "ACCESS GRANTED",
//             "WELCOME"
//         );
//     }


//     // ========================================================
//     // UNKNOWN CARD
//     // ========================================================

//     else
//     {
//         Serial.println(
//             "[RFID] ACCESS DENIED"
//         );


//         lcdShowMessage(
//             "ACCESS DENIED",
//             "UNKNOWN CARD"
//         );
//     }


//     Serial.println("========================================");


//     // Ghi nhận thời gian để sau 2 giây
//     // trả LCD về trạng thái hệ thống
//     rfidDisplayStart = millis();

//     rfidShowingMessage = true;
// }


// // ============================================================
// // HANDLE RFID LCD TIMEOUT
// // ============================================================

// void handleRFIDDisplayTimeout()
// {
//     if (!rfidShowingMessage)
//     {
//         return;
//     }


//     // Chưa đủ thời gian
//     if (
//         millis() - rfidDisplayStart
//         < RFID_MESSAGE_TIME
//     )
//     {
//         return;
//     }


//     rfidShowingMessage = false;


//     // ========================================================
//     // RESTORE LCD
//     // ========================================================

//     // Nếu GPS đã có vị trí
//     if (gpsValid)
//     {
//         showGPSOnLCD();
//     }

//     // Nếu GPS chưa có vị trí
//     else
//     {
//         lcdShowMessage(
//             "SYSTEM READY",
//             "SCAN RFID"
//         );
//     }
// }


// // ============================================================
// // SETUP
// // ============================================================

// void setup()
// {
//     // ========================================================
//     // SERIAL MONITOR
//     // ========================================================

//     Serial.begin(115200);

//     delay(2000);


//     Serial.println();
//     Serial.println("========================================");
//     Serial.println(" ESP32-S3 FULL SYSTEM");
//     Serial.println("========================================");

//     Serial.println("Camera : OV3660");
//     Serial.println("LCD    : I2C GPIO1 / GPIO2");
//     Serial.println("GPS    : UART1 GPIO41 / GPIO42");
//     Serial.println("A7680C : UART2 GPIO21 / GPIO47");
//     Serial.println("RFID   : RC522");

//     Serial.println("========================================");


//     // ========================================================
//     // 1. EVENT GROUP
//     // ========================================================

//     Serial.println();
//     Serial.println("----------------------------------------");
//     Serial.println("[SYSTEM] EVENT GROUP");
//     Serial.println("----------------------------------------");


//     if (!initSystemEvents())
//     {
//         Serial.println(
//             "[FATAL] EVENT GROUP FAILED"
//         );


//         while (1)
//         {
//             delay(1000);
//         }
//     }


//     Serial.println(
//         "[OK] EVENT GROUP"
//     );


//     // ========================================================
//     // 2. LCD
//     // ========================================================

//     Serial.println();
//     Serial.println("----------------------------------------");
//     Serial.println("[SYSTEM] LCD INIT");
//     Serial.println("----------------------------------------");


//     if (!initLCD())
//     {
//         Serial.println(
//             "[WARNING] LCD INIT FAILED"
//         );
//     }
//     else
//     {
//         Serial.println(
//             "[OK] LCD READY"
//         );


//         lcdShowMessage(
//             "SYSTEM START",
//             "PLEASE WAIT..."
//         );
//     }


//     // ========================================================
//     // 3. RFID RC522
//     // ========================================================

//     Serial.println();
//     Serial.println("----------------------------------------");
//     Serial.println("[SYSTEM] RFID INIT");
//     Serial.println("----------------------------------------");


//     if (!initRFID())
//     {
//         Serial.println(
//             "[WARNING] RFID INIT FAILED"
//         );


//         rfidReady = false;


//         lcdShowMessage(
//             "RFID ERROR",
//             "CHECK RC522"
//         );


//         delay(1500);
//     }
//     else
//     {
//         Serial.println(
//             "[OK] RFID READY"
//         );


//         rfidReady = true;


//         lcdShowMessage(
//             "RFID READY",
//             "SYSTEM START"
//         );


//         delay(1000);
//     }


//     // ========================================================
//     // 4. CAMERA
//     // ========================================================

//     Serial.println();
//     Serial.println("----------------------------------------");
//     Serial.println("[SYSTEM] CAMERA INIT");
//     Serial.println("----------------------------------------");


//     if (!initCamera())
//     {
//         Serial.println(
//             "[WARNING] CAMERA INIT FAILED"
//         );


//         lcdShowMessage(
//             "CAMERA ERROR",
//             "CHECK CAMERA"
//         );
//     }
//     else
//     {
//         Serial.println(
//             "[OK] CAMERA READY"
//         );


//         // Báo Camera đã sẵn sàng
//         xEventGroupSetBits(
//             systemEvents,
//             BIT_CAMERA_READY
//         );
//     }


//     // ========================================================
//     // 5. WIFI
//     // ========================================================

//     Serial.println();
//     Serial.println("----------------------------------------");
//     Serial.println("[SYSTEM] WIFI CONNECT");
//     Serial.println("----------------------------------------");


//     if (!initWiFi())
//     {
//         Serial.println(
//             "[WARNING] WIFI FAILED"
//         );


//         lcdShowMessage(
//             "WIFI ERROR",
//             "OFFLINE MODE"
//         );
//     }
//     else
//     {
//         Serial.println(
//             "[OK] WIFI CONNECTED"
//         );


//         // Báo Wi-Fi đã sẵn sàng
//         xEventGroupSetBits(
//             systemEvents,
//             BIT_WIFI_READY
//         );


//         // ====================================================
//         // CAMERA WEB SERVER
//         // ====================================================

//         Serial.println();

//         Serial.println(
//             "[SYSTEM] START CAMERA SERVER"
//         );


//         if (startCameraServer())
//         {
//             Serial.println(
//                 "[OK] CAMERA SERVER STARTED"
//             );
//         }
//         else
//         {
//             Serial.println(
//                 "[WARNING] CAMERA SERVER FAILED"
//             );
//         }
//     }


//     // ========================================================
//     // 6. CREATE GPS TASK
//     // ========================================================

//     Serial.println();
//     Serial.println("----------------------------------------");
//     Serial.println("[SYSTEM] CREATE GPS TASK");
//     Serial.println("----------------------------------------");


//     if (!createTaskGPS())
//     {
//         Serial.println(
//             "[WARNING] GPS TASK FAILED"
//         );
//     }
//     else
//     {
//         Serial.println(
//             "[OK] GPS TASK CREATED"
//         );
//     }


//     // ========================================================
//     // 7. CREATE GSM TASK
//     // ========================================================

//     Serial.println();
//     Serial.println("----------------------------------------");
//     Serial.println("[SYSTEM] CREATE GSM TASK");
//     Serial.println("----------------------------------------");


//     if (!createTaskGSM())
//     {
//         Serial.println(
//             "[WARNING] GSM TASK FAILED"
//         );
//     }
//     else
//     {
//         Serial.println(
//             "[OK] GSM TASK CREATED"
//         );
//     }


//     // ========================================================
//     // 8. WAIT FOR TASK INITIALIZATION
//     // ========================================================

//     Serial.println();
//     Serial.println("----------------------------------------");
//     Serial.println("[SYSTEM] WAIT TASKS");
//     Serial.println("----------------------------------------");


//     lcdShowMessage(
//         "GPS + GSM",
//         "STARTING..."
//     );


//     // Cho GPS + A7680C thời gian khởi động
//     delay(8000);


//     // ========================================================
//     // 9. REQUEST GPS
//     // ========================================================

//     Serial.println();
//     Serial.println("========================================");
//     Serial.println("[MAIN] REQUEST GPS");
//     Serial.println("========================================");


//     // Cho phép xử lý GPS mới
//     gpsDisplayed = false;


//     // Xóa trạng thái GPS cũ
//     xEventGroupClearBits(
//         systemEvents,
//         BIT_GPS_READY
//     );


//     // Yêu cầu task GPS lấy vị trí
//     xEventGroupSetBits(
//         systemEvents,
//         BIT_NEED_GPS
//     );


//     // ========================================================
//     // KHÔNG REQUEST GSM Ở ĐÂY
//     // ========================================================
//     //
//     // GSM chỉ được yêu cầu sau khi GPS có tọa độ hợp lệ.
//     //
//     // ========================================================


//     Serial.println();
//     Serial.println("========================================");
//     Serial.println(" SYSTEM INITIALIZATION COMPLETE");
//     Serial.println("========================================");


//     // Nếu RFID hoạt động
//     if (rfidReady)
//     {
//         lcdShowMessage(
//             "SYSTEM READY",
//             "SCAN RFID"
//         );
//     }
// }


// // ============================================================
// // LOOP
// // ============================================================

// void loop()
// {
//     // ========================================================
//     // 1. RFID
//     // ========================================================

//     // Kiểm tra thẻ liên tục
//     handleRFID();


//     // Kiểm tra thời gian hiển thị thông báo RFID
//     handleRFIDDisplayTimeout();


//     // ========================================================
//     // 2. READ EVENT BITS
//     // ========================================================

//     EventBits_t bits =
//         xEventGroupGetBits(
//             systemEvents
//         );


//     // ========================================================
//     // 3. GPS READY EVENT
//     // ========================================================

//     if (
//         (bits & BIT_GPS_READY) &&
//         !gpsDisplayed
//     )
//     {
//         // Đánh dấu đã xử lý GPS này
//         gpsDisplayed = true;


//         Serial.println();
//         Serial.println("========================================");
//         Serial.println("[MAIN] GPS READY");
//         Serial.println("========================================");


//         // ====================================================
//         // GPS VALID
//         // ====================================================

//         if (gpsValid)
//         {
//             // ================================================
//             // PRINT LATITUDE
//             // ================================================

//             Serial.print(
//                 "LAT : "
//             );

//             Serial.println(
//                 gpsLat,
//                 6
//             );


//             // ================================================
//             // PRINT LONGITUDE
//             // ================================================

//             Serial.print(
//                 "LNG : "
//             );

//             Serial.println(
//                 gpsLng,
//                 6
//             );


//             // ================================================
//             // PRINT SATELLITES
//             // ================================================

//             Serial.print(
//                 "SAT : "
//             );

//             Serial.println(
//                 gpsSatellites
//             );


//             // ================================================
//             // GOOGLE MAPS LINK
//             // ================================================

//             Serial.println();

//             Serial.println(
//                 "Google Maps:"
//             );


//             Serial.print(
//                 "https://maps.google.com/?q="
//             );

//             Serial.print(
//                 gpsLat,
//                 6
//             );

//             Serial.print(",");

//             Serial.println(
//                 gpsLng,
//                 6
//             );


//             // ================================================
//             // SHOW LOCATION ON LCD
//             // ================================================

//             // Không ghi đè LCD nếu RFID đang hiển thị
//             if (!rfidShowingMessage)
//             {
//                 showGPSOnLCD();
//             }


//             // ================================================
//             // REQUEST GSM
//             // ================================================
//             //
//             // Chỉ khi GPS hợp lệ mới yêu cầu taskGSM.
//             //
//             // taskGSM sẽ nhận BIT_NEED_SIM
//             // và gửi SMS chứa GPS.
//             //
//             // ================================================

//             Serial.println();
//             Serial.println("----------------------------------------");
//             Serial.println("[MAIN] GPS VALID");
//             Serial.println("[MAIN] REQUEST GSM SEND GPS SMS");
//             Serial.println("----------------------------------------");


//             xEventGroupSetBits(
//                 systemEvents,
//                 BIT_NEED_SIM
//             );
//         }


//         // ====================================================
//         // GPS INVALID
//         // ====================================================

//         else
//         {
//             Serial.println(
//                 "[MAIN] GPS INVALID"
//             );


//             // Không ghi đè thông báo RFID
//             if (!rfidShowingMessage)
//             {
//                 lcdShowMessage(
//                     "GPS ERROR",
//                     "NO LOCATION"
//                 );
//             }


//             // Không set BIT_NEED_SIM
//             // vì không muốn gửi SMS có tọa độ sai.
//         }


//         // ====================================================
//         // CLEAR GPS READY
//         // ====================================================

//         xEventGroupClearBits(
//             systemEvents,
//             BIT_GPS_READY
//         );


//         Serial.println();
//         Serial.println("========================================");
//         Serial.println("[MAIN] GPS EVENT COMPLETE");
//         Serial.println("========================================");
//     }


//     // ========================================================
//     // 4. FREE CPU
//     // ========================================================

//     vTaskDelay(
//         pdMS_TO_TICKS(50)
//     );
// }

#include <Arduino.h>

// ============================================================
// A7680C UART
// ============================================================

#define GSM_RX_PIN 21
#define GSM_TX_PIN 47
#define GSM_BAUD   115200

HardwareSerial modem(2);


// ============================================================
// APN
//
// ĐỂ TRỐNG trước.
// Nếu mạng của bạn yêu cầu APN cụ thể, điền APN chính thức
// của SIM vào đây.
//
// Ví dụ:
// const char *APN = "your_apn";
//
// ============================================================

const char *APN = "";


// ============================================================
// TEST HTTP URL
//
// Dùng HTTP trước để xác nhận data path.
// Sau khi PASS mới chuyển sang HTTPS/Azure.
// ============================================================

const char *TEST_URL = "http://example.com/";


// ============================================================
// CLEAR UART BUFFER
// ============================================================

void clearModemBuffer()
{
    while (modem.available())
    {
        modem.read();
    }
}


// ============================================================
// READ MODEM RESPONSE
// ============================================================

String readModemResponse(uint32_t timeoutMs)
{
    String response = "";

    uint32_t startTime = millis();

    while (millis() - startTime < timeoutMs)
    {
        while (modem.available())
        {
            char c = modem.read();

            response += c;

            Serial.write(c);

            // Có dữ liệu thì cho modem thêm thời gian trả phần còn lại
            startTime = millis();
        }

        delay(5);
    }

    return response;
}


// ============================================================
// SEND AT
// ============================================================

String sendAT(
    const String &command,
    uint32_t timeoutMs = 2000
)
{
    clearModemBuffer();

    Serial.println();
    Serial.println("--------------------------------");
    Serial.print(">> ");
    Serial.println(command);

    modem.println(command);

    String response =
        readModemResponse(timeoutMs);

    Serial.println();

    return response;
}


// ============================================================
// CHECK RESPONSE
// ============================================================

bool responseContains(
    const String &response,
    const char *text
)
{
    return response.indexOf(text) >= 0;
}


// ============================================================
// WAIT FOR MODEM
// ============================================================

bool waitForModem()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 1 - MODEM");
    Serial.println("================================");

    for (int attempt = 1; attempt <= 5; attempt++)
    {
        Serial.print("[MODEM] Attempt ");
        Serial.println(attempt);

        String response =
            sendAT("AT", 1000);

        if (responseContains(response, "OK"))
        {
            Serial.println("[PASS] MODEM RESPONDING");

            return true;
        }

        delay(1000);
    }

    Serial.println("[FAIL] MODEM NOT RESPONDING");

    return false;
}


// ============================================================
// CHECK SIM
// ============================================================

bool checkSIM()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 2 - SIM");
    Serial.println("================================");

    String response =
        sendAT("AT+CPIN?", 2000);

    if (responseContains(response, "READY"))
    {
        Serial.println("[PASS] SIM READY");

        return true;
    }

    Serial.println("[FAIL] SIM NOT READY");

    return false;
}


// ============================================================
// CHECK SIGNAL
// ============================================================

void checkSignal()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 3 - SIGNAL");
    Serial.println("================================");

    sendAT(
        "AT+CSQ",
        2000
    );
}


// ============================================================
// CHECK NETWORK REGISTRATION
// ============================================================

bool checkNetwork()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 4 - LTE NETWORK");
    Serial.println("================================");

    for (int attempt = 1; attempt <= 10; attempt++)
    {
        String response =
            sendAT(
                "AT+CEREG?",
                2000
            );

        // Registered home
        if (
            response.indexOf(",1") >= 0
        )
        {
            Serial.println(
                "[PASS] REGISTERED HOME NETWORK"
            );

            return true;
        }

        // Registered roaming
        if (
            response.indexOf(",5") >= 0
        )
        {
            Serial.println(
                "[PASS] REGISTERED ROAMING"
            );

            return true;
        }

        Serial.println(
            "[WAIT] Network registration..."
        );

        delay(2000);
    }

    Serial.println(
        "[FAIL] NOT REGISTERED TO LTE NETWORK"
    );

    return false;
}


// ============================================================
// CHECK OPERATOR
// ============================================================

void checkOperator()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 5 - OPERATOR");
    Serial.println("================================");

    sendAT(
        "AT+COPS?",
        3000
    );
}


// ============================================================
// CHECK PACKET SERVICE
// ============================================================

void checkPacketService()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 6 - PACKET SERVICE");
    Serial.println("================================");

    sendAT(
        "AT+CGATT?",
        3000
    );

    sendAT(
        "AT+CGDCONT?",
        3000
    );
}


// ============================================================
// CONFIGURE APN
// ============================================================

void configureAPN()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 7 - APN");
    Serial.println("================================");

    if (strlen(APN) == 0)
    {
        Serial.println(
            "[INFO] APN variable is empty."
        );

        Serial.println(
            "[INFO] Keeping SIM/modem PDP configuration."
        );

        return;
    }

    String command =
        "AT+CGDCONT=1,\"IP\",\"";

    command += APN;

    command += "\"";

    sendAT(
        command,
        3000
    );
}


// ============================================================
// ACTIVATE NETWORK
// ============================================================

bool activateNetwork()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 8 - ACTIVATE 4G DATA");
    Serial.println("================================");

    // Attach packet service
    sendAT(
        "AT+CGATT=1",
        5000
    );

    delay(1000);


    // Activate PDP/network context
    String response =
        sendAT(
            "AT+NETOPEN",
            10000
        );


    // Một số firmware trả OK trước,
    // sau đó mới +NETOPEN: 0.
    if (
        response.indexOf("+NETOPEN: 0") >= 0 ||
        response.indexOf("Network is already opened") >= 0
    )
    {
        Serial.println(
            "[PASS] MOBILE DATA ACTIVE"
        );

        return true;
    }


    // Kiểm tra trạng thái NETOPEN
    response =
        sendAT(
            "AT+NETOPEN?",
            3000
        );


    if (
        response.indexOf("+NETOPEN: 1") >= 0
    )
    {
        Serial.println(
            "[PASS] MOBILE DATA ACTIVE"
        );

        return true;
    }


    Serial.println(
        "[WARNING] Could not confirm NETOPEN."
    );

    return false;
}


// ============================================================
// CHECK IP ADDRESS
// ============================================================

void checkIPAddress()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 9 - IP ADDRESS");
    Serial.println("================================");

    sendAT(
        "AT+IPADDR",
        3000
    );
}


// ============================================================
// HTTP GET
// ============================================================

bool httpGET()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" STEP 10 - HTTP GET");
    Serial.println("================================");


    // --------------------------------------------------------
    // Cleanup previous HTTP session
    // --------------------------------------------------------

    sendAT(
        "AT+HTTPTERM",
        2000
    );


    // --------------------------------------------------------
    // HTTP INIT
    // --------------------------------------------------------

    String response =
        sendAT(
            "AT+HTTPINIT",
            3000
        );


    if (!responseContains(response, "OK"))
    {
        Serial.println(
            "[FAIL] HTTPINIT"
        );

        return false;
    }


    // --------------------------------------------------------
    // CID
    // --------------------------------------------------------

    sendAT(
        "AT+HTTPPARA=\"CID\",1",
        3000
    );


    // --------------------------------------------------------
    // URL
    // --------------------------------------------------------

    String command =
        "AT+HTTPPARA=\"URL\",\"";

    command += TEST_URL;

    command += "\"";


    response =
        sendAT(
            command,
            3000
        );


    if (!responseContains(response, "OK"))
    {
        Serial.println(
            "[FAIL] SET HTTP URL"
        );

        sendAT(
            "AT+HTTPTERM",
            2000
        );

        return false;
    }


    // --------------------------------------------------------
    // HTTP GET
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "[HTTP] Sending GET..."
    );


    response =
        sendAT(
            "AT+HTTPACTION=0",
            15000
        );


    // HTTPACTION response thường có dạng:
    //
    // +HTTPACTION: 0,200,<length>
    //
    // 200 = HTTP OK
    // --------------------------------------------------------

    int actionIndex =
        response.indexOf("+HTTPACTION:");

    if (actionIndex < 0)
    {
        Serial.println(
            "[FAIL] NO HTTPACTION RESULT"
        );

        sendAT(
            "AT+HTTPTERM",
            2000
        );

        return false;
    }


    // --------------------------------------------------------
    // CHECK HTTP STATUS 200
    // --------------------------------------------------------

    if (
        response.indexOf(",200,") >= 0
    )
    {
        Serial.println();
        Serial.println(
            "[PASS] HTTP STATUS = 200"
        );


        // ----------------------------------------------------
        // READ SERVER RESPONSE
        // ----------------------------------------------------

        Serial.println();
        Serial.println(
            "[HTTP] Reading response..."
        );


        sendAT(
            "AT+HTTPREAD=0,500",
            5000
        );


        // ----------------------------------------------------
        // CLOSE HTTP
        // ----------------------------------------------------

        sendAT(
            "AT+HTTPTERM",
            3000
        );


        return true;
    }


    Serial.println();
    Serial.println(
        "[FAIL] HTTP STATUS IS NOT 200"
    );


    sendAT(
        "AT+HTTPTERM",
        3000
    );


    return false;
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(2000);


    Serial.println();
    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        " ESP32-S3 + A7680C"
    );

    Serial.println(
        " 4G INTERNET + HTTP TEST"
    );

    Serial.println(
        "========================================"
    );


    Serial.print(
        "ESP32 RX : GPIO"
    );

    Serial.println(
        GSM_RX_PIN
    );


    Serial.print(
        "ESP32 TX : GPIO"
    );

    Serial.println(
        GSM_TX_PIN
    );


    Serial.print(
        "UART     : "
    );

    Serial.println(
        GSM_BAUD
    );


    // ========================================================
    // START MODEM UART
    // ========================================================

    modem.begin(
        GSM_BAUD,
        SERIAL_8N1,
        GSM_RX_PIN,
        GSM_TX_PIN
    );


    delay(3000);


    // ========================================================
    // 1. MODEM
    // ========================================================

    if (!waitForModem())
    {
        Serial.println();
        Serial.println(
            "STOP: CHECK MODEM UART"
        );

        return;
    }


    // ========================================================
    // BASIC CONFIG
    // ========================================================

    sendAT(
        "ATE0",
        2000
    );


    // ========================================================
    // 2. SIM
    // ========================================================

    if (!checkSIM())
    {
        Serial.println();
        Serial.println(
            "STOP: CHECK SIM"
        );

        return;
    }


    // ========================================================
    // 3. SIGNAL
    // ========================================================

    checkSignal();


    // ========================================================
    // 4. NETWORK
    // ========================================================

    if (!checkNetwork())
    {
        Serial.println();
        Serial.println(
            "STOP: LTE NETWORK NOT READY"
        );

        return;
    }


    // ========================================================
    // 5. OPERATOR
    // ========================================================

    checkOperator();


    // ========================================================
    // 6. PACKET SERVICE
    // ========================================================

    checkPacketService();


    // ========================================================
    // 7. APN
    // ========================================================

    configureAPN();


    // ========================================================
    // 8. ACTIVATE MOBILE DATA
    // ========================================================

    bool dataActive =
        activateNetwork();


    // Dù NETOPEN parsing không xác nhận được,
    // vẫn in trạng thái để debug.
    Serial.print(
        "[DATA] NETOPEN result: "
    );

    Serial.println(
        dataActive ? "ACTIVE" : "CHECK REQUIRED"
    );


    // ========================================================
    // 9. IP
    // ========================================================

    checkIPAddress();


    // ========================================================
    // 10. HTTP
    // ========================================================

    bool httpOK =
        httpGET();


    // ========================================================
    // FINAL RESULT
    // ========================================================

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        " FINAL RESULT"
    );

    Serial.println(
        "========================================"
    );


    if (httpOK)
    {
        Serial.println(
            "A7680C 4G INTERNET : PASS"
        );

        Serial.println(
            "HTTP GET            : PASS"
        );

        Serial.println();
        Serial.println(
            "READY FOR AZURE TEST"
        );
    }
    else
    {
        Serial.println(
            "HTTP GET : FAIL"
        );

        Serial.println();
        Serial.println(
            "Check the AT responses above."
        );
    }


    Serial.println(
        "========================================"
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // Forward unsolicited modem messages to Serial Monitor
    // --------------------------------------------------------

    while (modem.available())
    {
        Serial.write(
            modem.read()
        );
    }


    delay(10);
}