#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "drivers/lcd.h"

#include "config/pins.h"
#include "config/settings.h"


LiquidCrystal_I2C lcd(
    LCD_I2C_ADDRESS,
    LCD_COLUMNS,
    LCD_ROWS
);


bool initLCD()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" LCD");
    Serial.println("================================");


    Wire.begin(
        I2C_SDA_PIN,
        I2C_SCL_PIN
    );


    lcd.init();

    lcd.backlight();

    lcd.clear();


    lcd.setCursor(0, 0);
    lcd.print("ESP32 LOGICTIC");

    lcd.setCursor(0, 1);
    lcd.print("Starting...");


    Serial.println("[OK] LCD initialized");

    return true;
}


void lcdShowMessage(
    const char *line1,
    const char *line2
)
{
    lcd.clear();


    lcd.setCursor(0, 0);
    lcd.print(line1);


    lcd.setCursor(0, 1);
    lcd.print(line2);
}