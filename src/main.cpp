#include "app.h"
#include "lcd.h"

I2C_LCD_HandleTypeDef lcd1 = {&hi2c1, 0x27 << 1};

void setup(void)
{
    lcd_init(&lcd1);

    lcd_print(&lcd1, 0, "Hello, World!");
    lcd_print(&lcd1, 1, "ECHO 20x4 LCD test");
    lcd_print(&lcd1, 3, "12345678901234567890");
}

void loop(void)
{
}
