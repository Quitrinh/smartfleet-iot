#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

// Khởi tạo A7680C
bool initGSM();

// Kiểm tra SIM / tín hiệu / mạng
bool testGSM();

// Gửi AT command
bool sendATCommand(
    const char *command,
    const char *expected,
    uint32_t timeoutMs
);

// Gửi SMS tới một số
bool sendSMS(
    const char *phone,
    const String &message
);

// Tạo nội dung GPS
String createGPSMessage();

// Gửi GPS tới một số
bool sendGPSSMS(
    const char *phone
);

// Gửi GPS tới toàn bộ số trong smsPhones[]
bool sendGPSToAllPhones();

#endif