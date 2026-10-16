#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#include "drivers/rfid.h"
#include "config/pins.h"

// ============================================================
// RC522
// ============================================================

static MFRC522 rfid(
    RFID_SS_PIN,
    RFID_RST_PIN
);


// ============================================================
// INIT RFID
// ============================================================

bool initRFID()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" RFID RC522 INIT");
    Serial.println("================================");

    Serial.print("SS   GPIO : ");
    Serial.println(RFID_SS_PIN);

    Serial.print("RST  GPIO : ");
    Serial.println(RFID_RST_PIN);

    Serial.print("SCK  GPIO : ");
    Serial.println(RFID_SCK_PIN);

    Serial.print("MOSI GPIO : ");
    Serial.println(RFID_MOSI_PIN);

    Serial.print("MISO GPIO : ");
    Serial.println(RFID_MISO_PIN);


    // Khởi tạo SPI với chân tự chọn
    SPI.begin(
        RFID_SCK_PIN,
        RFID_MISO_PIN,
        RFID_MOSI_PIN,
        RFID_SS_PIN
    );


    delay(100);


    // Khởi tạo RC522
    rfid.PCD_Init();

    delay(100);


    // Đọc version register để kiểm tra RC522
    byte version =
        rfid.PCD_ReadRegister(
            MFRC522::VersionReg
        );


    Serial.print("[RFID] VersionReg: 0x");
    Serial.println(
        version,
        HEX
    );


    // 0x00 hoặc 0xFF thường cho thấy
    // ESP32 không giao tiếp được với RC522
    if (
        version == 0x00 ||
        version == 0xFF
    )
    {
        Serial.println(
            "[RFID ERROR] RC522 NOT DETECTED"
        );

        return false;
    }


    Serial.println(
        "[OK] RC522 READY"
    );

    Serial.println(
        "[RFID] Place card near reader..."
    );


    return true;
}


// ============================================================
// READ RFID
// ============================================================

bool readRFID(String &uid)
{
    // Chưa có thẻ
    if (!rfid.PICC_IsNewCardPresent())
    {
        return false;
    }


    // Không đọc được UID
    if (!rfid.PICC_ReadCardSerial())
    {
        return false;
    }


    uid = "";


    // ========================================================
    // BUILD UID STRING
    // ========================================================

    for (
        byte i = 0;
        i < rfid.uid.size;
        i++
    )
    {
        if (rfid.uid.uidByte[i] < 0x10)
        {
            uid += "0";
        }


        uid += String(
            rfid.uid.uidByte[i],
            HEX
        );


        if (i < rfid.uid.size - 1)
        {
            uid += " ";
        }
    }


    uid.toUpperCase();


    // ========================================================
    // STOP COMMUNICATION
    // ========================================================

    rfid.PICC_HaltA();

    rfid.PCD_StopCrypto1();


    return true;
}