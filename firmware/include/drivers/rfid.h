#ifndef RFID_H
#define RFID_H

#include <Arduino.h>

bool initRFID();
bool readRFID(String &uid);

#endif