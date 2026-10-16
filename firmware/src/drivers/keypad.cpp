#include <Arduino.h>
#include <Wire.h>

#include "drivers/keypad.h"
#include "config/pins.h"

// ============================================================
// PCF8574 KEYPAD ADDRESS
// ============================================================

#define KEYPAD_PCF8574_ADDR 0x20


// ============================================================
// KEY MAP
//
// Mapping thực tế của keypad đã test:
//
// 1 2 3 A
// 4 5 6 B
// 7 8 9 C
// * 0 # D
// ============================================================

static const char keys[4][4] =
{
    {'D', 'C', 'B', 'A'},
    {'#', '9', '6', '3'},
    {'0', '8', '5', '2'},
    {'*', '7', '4', '1'}
};


// ============================================================
// PCF8574 WRITE
// ============================================================

static void pcfWrite(uint8_t data)
{
    Wire.beginTransmission(KEYPAD_PCF8574_ADDR);

    Wire.write(data);

    Wire.endTransmission();
}


// ============================================================
// PCF8574 READ
// ============================================================

static uint8_t pcfRead()
{
    Wire.requestFrom(KEYPAD_PCF8574_ADDR, 1);

    if (Wire.available())
    {
        return Wire.read();
    }

    return 0xFF;
}


// ============================================================
// CHECK CONNECTION
// ============================================================

bool isKeypadConnected()
{
    Wire.beginTransmission(KEYPAD_PCF8574_ADDR);

    return Wire.endTransmission() == 0;
}


// ============================================================
// INIT KEYPAD
// ============================================================

bool initKeypad()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" KEYPAD PCF8574");
    Serial.println("================================");

    // LCD cũng dùng Wire nên gọi lại với cùng SDA/SCL là được.
    Wire.begin(
        I2C_SDA_PIN,
        I2C_SCL_PIN
    );

    delay(50);

    if (!isKeypadConnected())
    {
        Serial.println(
            "[ERROR] PCF8574 KEYPAD NOT FOUND"
        );

        return false;
    }

    pcfWrite(0xFF);

    Serial.print(
        "[OK] PCF8574 FOUND : 0x"
    );

    Serial.println(
        KEYPAD_PCF8574_ADDR,
        HEX
    );

    Serial.println(
        "[OK] KEYPAD READY"
    );

    return true;
}


// ============================================================
// READ KEYPAD
// ============================================================

char readKeypad()
{
    for (int row = 0; row < 4; row++)
    {
        // ----------------------------------------------------
        // ALL HIGH
        // ----------------------------------------------------

        uint8_t output = 0xFF;


        // ----------------------------------------------------
        // CURRENT ROW LOW
        // ----------------------------------------------------

        output &= ~(1 << row);

        pcfWrite(output);

        delayMicroseconds(200);


        // ----------------------------------------------------
        // READ PCF8574
        // ----------------------------------------------------

        uint8_t input = pcfRead();


        // ----------------------------------------------------
        // CHECK COLUMNS P4-P7
        // ----------------------------------------------------

        for (int col = 0; col < 4; col++)
        {
            int columnPin = col + 4;


            if ((input & (1 << columnPin)) == 0)
            {
                // ============================================
                // DEBOUNCE
                // ============================================

                delay(20);

                input = pcfRead();


                if ((input & (1 << columnPin)) == 0)
                {
                    char key =
                        keys[row][col];


                    // ========================================
                    // WAIT UNTIL KEY RELEASED
                    // ========================================

                    while (
                        (pcfRead() & (1 << columnPin)) == 0
                    )
                    {
                        delay(5);
                    }


                    pcfWrite(0xFF);

                    return key;
                }
            }
        }
    }


    pcfWrite(0xFF);

    return '\0';
}