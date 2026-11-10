#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "drivers/output.h"
#include "config/pins.h"


// ============================================================
// RGB LED CONFIG
// ESP32-S3 onboard RGB LED
// GPIO48
// ============================================================

static Adafruit_NeoPixel rgbLed(
    1,
    RGB_LED_PIN,
    NEO_GRB + NEO_KHZ800
);


// ============================================================
// OUTPUT STATE
// ============================================================

static bool outputReady = false;


// ============================================================
// INITIALIZATION
// ============================================================

bool initOutput()
{
    Serial.println();
    Serial.println("[INIT] RGB LED / Buzzer...");


    // ========================================================
    // BUZZER - GPIO45
    // ========================================================

    pinMode(
        BUZZER_PIN,
        OUTPUT
    );

    digitalWrite(
        BUZZER_PIN,
        LOW
    );


    // ========================================================
    // RGB LED - GPIO48
    // ========================================================

    rgbLed.begin();

    rgbLed.clear();

    rgbLed.show();


    outputReady = true;


    Serial.println(
        "[OK] RGB LED GPIO48 ready."
    );

    Serial.println(
        "[OK] Buzzer GPIO45 ready."
    );


    // ========================================================
    // STARTUP TEST
    //
    // BLUE -> OFF -> BEEP
    // ========================================================

    ledBlue();

    delay(200);

    ledOff();


    buzzerBeep(60);


    Serial.println(
        "[OK] Output initialization completed."
    );


    return true;
}


// ============================================================
// SET RGB COLOR
// ============================================================

void ledSetColor(
    uint8_t red,
    uint8_t green,
    uint8_t blue
)
{
    if (!outputReady)
    {
        return;
    }


    uint32_t color = rgbLed.Color(
        red,
        green,
        blue
    );


    rgbLed.setPixelColor(
        0,
        color
    );


    rgbLed.show();
}


// ============================================================
// LED ON
//
// Default color = GREEN
// ============================================================

void ledOn()
{
    if (!outputReady)
    {
        return;
    }


    ledGreen();
}


// ============================================================
// LED OFF
// ============================================================

void ledOff()
{
    if (!outputReady)
    {
        return;
    }


    rgbLed.setPixelColor(
        0,
        0
    );


    rgbLed.show();
}


// ============================================================
// RED
// ============================================================

void ledRed()
{
    if (!outputReady)
    {
        return;
    }


    ledSetColor(
        255,
        0,
        0
    );
}


// ============================================================
// GREEN
// ============================================================

void ledGreen()
{
    if (!outputReady)
    {
        return;
    }


    ledSetColor(
        0,
        255,
        0
    );
}


// ============================================================
// BLUE
// ============================================================

void ledBlue()
{
    if (!outputReady)
    {
        return;
    }


    ledSetColor(
        0,
        0,
        255
    );
}


// ============================================================
// YELLOW
// ============================================================

void ledYellow()
{
    if (!outputReady)
    {
        return;
    }


    ledSetColor(
        255,
        180,
        0
    );
}


// ============================================================
// NORMAL LED BLINK
//
// GREEN
// ============================================================

void ledBlink(
    uint8_t times,
    uint16_t onMs,
    uint16_t offMs
)
{
    if (!outputReady)
    {
        return;
    }


    for (
        uint8_t i = 0;
        i < times;
        ++i
    )
    {
        ledGreen();


        delay(onMs);


        ledOff();


        if (
            i + 1 < times
        )
        {
            delay(offMs);
        }
    }


    ledOff();
}


// ============================================================
// ERROR LED BLINK
//
// RED
// ============================================================

void ledBlinkError(
    uint8_t times,
    uint16_t onMs,
    uint16_t offMs
)
{
    if (!outputReady)
    {
        return;
    }


    for (
        uint8_t i = 0;
        i < times;
        ++i
    )
    {
        ledRed();


        delay(onMs);


        ledOff();


        if (
            i + 1 < times
        )
        {
            delay(offMs);
        }
    }


    ledOff();
}


// ============================================================
// BUZZER ON
//
// GPIO45
// ============================================================

void buzzerOn()
{
    if (!outputReady)
    {
        return;
    }


    digitalWrite(
        BUZZER_PIN,
        HIGH
    );
}


// ============================================================
// BUZZER OFF
// ============================================================

void buzzerOff()
{
    if (!outputReady)
    {
        return;
    }


    digitalWrite(
        BUZZER_PIN,
        LOW
    );
}


// ============================================================
// GENERIC BEEP
// ============================================================

void buzzerBeep(
    uint16_t durationMs
)
{
    if (!outputReady)
    {
        return;
    }


    buzzerOn();


    delay(durationMs);


    buzzerOff();
}


// ============================================================
// RFID BEEP
//
// One short beep
// ============================================================

void buzzerRFID()
{
    if (!outputReady)
    {
        return;
    }


    buzzerBeep(90);
}


// ============================================================
// KEYPAD BEEP
//
// One very short beep
// ============================================================

void buzzerKey()
{
    if (!outputReady)
    {
        return;
    }


    buzzerBeep(35);
}


// ============================================================
// CANCEL
//
// Beep - Beep
// ============================================================

void buzzerCancel()
{
    if (!outputReady)
    {
        return;
    }


    buzzerBeep(70);


    delay(60);


    buzzerBeep(70);
}


// ============================================================
// SUCCESS
//
// Short beep
// Long beep
// ============================================================

void buzzerSuccess()
{
    if (!outputReady)
    {
        return;
    }


    buzzerBeep(80);


    delay(70);


    buzzerBeep(160);
}


// ============================================================
// ERROR
//
// Beep x3
// ============================================================

void buzzerError()
{
    if (!outputReady)
    {
        return;
    }


    for (
        uint8_t i = 0;
        i < 3;
        ++i
    )
    {
        buzzerBeep(120);


        if (
            i < 2
        )
        {
            delay(80);
        }
    }
}


// ============================================================
// AUTH SUCCESS
//
// BUZZER:
//      short + long
//
// RGB:
//      GREEN x3
//
// END:
//      RGB OFF
// ============================================================

void outputSuccess()
{
    if (!outputReady)
    {
        return;
    }


    Serial.println(
        "[OUTPUT] AUTH SUCCESS"
    );


    buzzerSuccess();


    ledBlink(
        3,
        120,
        120
    );


    ledOff();
}


// ============================================================
// AUTH ERROR
//
// BUZZER:
//      x3
//
// RGB:
//      RED x3
//
// END:
//      RGB OFF
// ============================================================

void outputError()
{
    if (!outputReady)
    {
        return;
    }


    Serial.println(
        "[OUTPUT] AUTH ERROR"
    );


    buzzerError();


    ledBlinkError(
        3,
        150,
        150
    );


    ledOff();
}