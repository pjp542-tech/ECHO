#include "app.h"
#include <cstdio>
#include <cstring>

extern "C" {
extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart2;
}

constexpr uint32_t R_REF = 10000; // 10k ohms

volatile uint32_t adc_raw = 0;
volatile uint32_t resistance_ohms = 0;

// Runs once after the board is ready.
void setup(void)
{
    // Calibrate ADC1
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure longer ADC sampling time
    ADC_ChannelConfTypeDef config = {};

    config.Channel = ADC_CHANNEL_1;
    config.Rank = ADC_REGULAR_RANK_1;
    config.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
    config.SingleDiff = ADC_SINGLE_ENDED;
    config.OffsetNumber = ADC_OFFSET_NONE;
    config.Offset = 0;

    if (HAL_ADC_ConfigChannel(&hadc1, &config) != HAL_OK)
    {
        Error_Handler();
    }
}

// Runs repeatedly.
void loop(void)
{
    
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        adc_raw = HAL_ADC_GetValue(&hadc1);

        char message[80];

        if (adc_raw < 4095)
        {
            resistance_ohms = (uint32_t)(
                (10000ULL * adc_raw) / (4095 - adc_raw)
            );

            snprintf(message, sizeof(message),
                "ADC: %lu | Resistance: %lu ohms\r\n",
                (unsigned long)adc_raw,
                (unsigned long)resistance_ohms);
        }
        else
        {
            snprintf(message, sizeof(message),
                "Resistance out of range\r\n");
        }

        std::printf("%s", message);
    }

    HAL_ADC_Stop(&hadc1);
    HAL_Delay(500);
}
