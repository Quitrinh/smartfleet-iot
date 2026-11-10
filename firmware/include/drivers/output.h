#ifndef OUTPUT_H
#define OUTPUT_H

#include <Arduino.h>

// ============================================================
// INITIALIZATION
// ============================================================

bool initOutput();


// ============================================================
// RGB LED - GPIO48
// ============================================================

// Set RGB color
void ledSetColor(
    uint8_t red,
    uint8_t green,
    uint8_t blue
);

// Default LED ON = GREEN
// Keep ledOn() for compatibility with existing code
void ledOn();

// Turn RGB completely OFF
void ledOff();

// Useful colors
void ledRed();
void ledGreen();
void ledBlue();
void ledYellow();

// Blink GREEN
void ledBlink(
    uint8_t times = 3,
    uint16_t onMs = 120,
    uint16_t offMs = 120
);

// Blink RED for error
void ledBlinkError(
    uint8_t times = 3,
    uint16_t onMs = 150,
    uint16_t offMs = 150
);


// ============================================================
// BUZZER - GPIO45
// ============================================================

void buzzerOn();
void buzzerOff();

void buzzerBeep(
    uint16_t durationMs = 45
);

void buzzerRFID();
void buzzerKey();
void buzzerCancel();

void buzzerSuccess();
void buzzerError();


// ============================================================
// COMBINED FEEDBACK
// ============================================================

// Authentication success:
// buzzer confirmation
// + GREEN RGB blink
// + RGB OFF at the end
void outputSuccess();

// Authentication/access error:
// error buzzer
// + RED RGB blink
// + RGB OFF at the end
void outputError();


#endif