#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>

// Khởi tạo PCF8574 + Keypad
bool initKeypad();

// Đọc một phím
// Không có phím -> '\0'
char readKeypad();

// Kiểm tra PCF8574 có tồn tại không
bool isKeypadConnected();

#endif