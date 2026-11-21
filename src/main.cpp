#include "app.h"
#include "meter_app.h"

extern "C" {
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;
}

namespace {
HardwareManager hardware(hadc1, htim2, hi2c1);
MeterApp meter_app(hardware);
}

void setup(void)
{
    if (!meter_app.begin())
        Error_Handler();
}

void loop(void)
{
    meter_app.update();
}
