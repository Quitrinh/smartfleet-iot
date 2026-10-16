#ifndef GPS_H
#define GPS_H

#include <Arduino.h>

bool initGPS();

bool readGPS(uint32_t timeoutMs);

uint32_t getGPSCharactersProcessed();

#endif