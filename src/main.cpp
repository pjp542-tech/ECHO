#include "app.h"
#include <cstdint>

extern "C" {
    extern ADC_HandleTypeDef hadc1;
}

// Known reference resistor
constexpr float R_REF = 10000.0f;  // 10 kΩ

// 63.2% of the maximum 12-bit ADC value
constexpr uint32_t ADC_THRESHOLD = 2588;

// Store measured values
uint32_t charge_time_us = 0;
float capacitance_F = 0.0f;

void setup(void)
{
    // Calibrate ADC
    if (HAL_ADCEx_Calibration_Start(
        &hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }
}