#include "app.h"
#include "resistance.h"
#include "inductance.h"
#include "capacitance.h"
#include "lcd.h"


I2C_LCD_HandleTypeDef lcd1 = {&hi2c1, 0x27 << 1};

typedef struct __TIM_HandleTypeDef TIM_HandleTypeDef;

extern "C" {
extern TIM_HandleTypeDef htim2;


extern "C" {
extern ADC_HandleTypeDef hadc1;
}

namespace
{
InductanceMeter i(htim2);
ResistanceMeter r(hadc1);
CapacitanceMeter c(hadc1);
}

void setup(void)
{
    if (i.activate() != InductanceMeter::Status::Ok)

    if (r.activate() != ResistanceMeter::Status::Ok)
    if (c.activate() != CapacitanceMeter::Status::Ok)
        Error_Handler();

    lcd_init(&lcd1);

}

void loop(void)
{
    float inductance = i.read_measurement();
    (void)inductance; // Use inductance in the component meter when ready.
    HAL_Delay(500); // Preserve the interval between LC excitation pulses.
    float resistance = r.read_measurement();
    (void)resistance; // Use resistance in the component meter when ready.
    float capacitance = c.read_measurement();
    (void)capacitance; // Use capacitance in the component meter when ready.
}
