#include "capacitance.h"
#include "app.h"

#include <cmath>

// sets handler for adc1
extern "C" {
    extern ADC_HandleTypeDef hadc1;
}

// <1 percent voltage threshold
const uint32_t Capacitor_empty = 20;
// 20 percent voltage threshold
const uint32_t V1 = 819;
// 80 percent voltage threshold
const uint32_t V2 = 3276;
// ln4
const float ln4 = std::log(4.0f);
// timeout vlaue about 6 seconds 
const uint32_t timeout = 1000000000;

void capacitance_init(void){
    // setup timer 
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    // initalise clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    // initalise d2
    GPIO_InitTypeDef pin = {};
    pin.Pin = GPIO_PIN_12;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull =GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOA, &pin);
    // initalise d3
    pin = {};
    pin.Pin = GPIO_PIN_0;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &pin);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    
}

// sets up read ADC at pin d3
static void select_sense_channel(void){
    ADC_ChannelConfTypeDef ch = {};
    ch.Channel = ADC_CHANNEL_15;
    ch.Rank = ADC_REGULAR_RANK_1;
    // Allow more acquisition time for the ADC input to settle.
    ch.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    ch.SingleDiff = ADC_SINGLE_ENDED;
    ch.OffsetNumber = ADC_OFFSET_NONE;
    HAL_ADC_ConfigChannel(&hadc1, &ch);
}

// reads ADC value
static uint32_t readAdc(void){
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1);
    uint32_t Adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return Adc_value;
}

// discharge cpacitor on d2
static void discharge_capacitor(void){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    uint32_t start = DWT->CYCCNT;
    while (readAdc() > Capacitor_empty){
        if (DWT->CYCCNT - start > timeout) return;
    }
}

// calculates capcitance from charge time and resitance
static float capacitance_from_time(float dt, float r){
    return dt / (r * ln4);
}


float capacitance_measure(void){

    // initliase vlaues for t1, t2 and voltage 
    uint32_t t1 = 0, t2 = 0;
    uint32_t v = 0;

    select_sense_channel();
    discharge_capacitor();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
    uint32_t start = DWT->CYCCNT;
    while (true){
        v = readAdc();
        if (t1 == 0 && v >= V1){
            t1 = DWT->CYCCNT;
        }
        if (v >= V2){
            t2 = DWT->CYCCNT;
            break;
        }
        if (DWT->CYCCNT - start > timeout) {
            discharge_capacitor();
            return 0.0f;
        }        
    }
    discharge_capacitor();
    // calculate capacitance 
    float dt = (float)(t2 -t1) / SystemCoreClock;
    float C = capacitance_from_time(dt, 10000.0f);
    
    return C;
}
