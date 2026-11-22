#ifndef ECHO_METER_APP_H
#define ECHO_METER_APP_H

#include "buttons.h"
#include "component_meter.h"
#include "lcd.h"

// Hardware setup is provided; application behaviour is left to the group.
class MeterApp {
public:
    explicit MeterApp(HardwareManager& hardware);
    // Initialise peripherals without selecting a meter; the menu will select one later.
    bool begin();
    void update();
    

private:
    // Highlight the zero-based menu choice (0..2) on LCD rows 1..3.
    void menu();
    void change_row(int row);
    void selected_meter(int choice);
    void get_measurement();
    void display_error(ComponentMeter::Result result);
    // Average 10 fresh readings, or return the first error result unchanged.
    ComponentMeter::Result avg_measurement();
    void display_result(const char* text);
    // Format as "4.70 k" or "22.00 n"; append the unit directly after the prefix.
    // Returned text remains valid until the next call, which overwrites it.
    const char* sanatise_result(float result);
    ComponentMeter component_meter_;
    Buttons buttons_;
    I2C_LCD_HandleTypeDef lcd_;
};

#endif
