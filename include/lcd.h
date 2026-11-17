#ifndef ECHO_LCD_H
#define ECHO_LCD_H

#include "i2c_lcd.h"

// Print at the start of row 0..3.
inline void lcd_print(I2C_LCD_HandleTypeDef *lcd, uint8_t row, const char *text)
{
    if (row >= 4 || text == nullptr) return;
    lcd_gotoxy(lcd, 0, row);
    lcd_puts(lcd, text);
}

#endif
