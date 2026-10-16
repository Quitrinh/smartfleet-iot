#ifndef LCD_H
#define LCD_H

bool initLCD();

void lcdShowMessage(
    const char *line1,
    const char *line2
);

#endif