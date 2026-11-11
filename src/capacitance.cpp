#include "capacitance.h"
#include "app.h"

#include <cmath>

// sets handler for adc1
extern "C" {
    extern ADC_HandleTypeDef hadc1;
}

typedef struct{
    GPIO_TypeDef *port;
    uint32_t pin;
    float ohms;
} pin_struct;

enum pin_id{
    pin_d1,
    pin_d2,
    pin_d6, 
    pin_a6
};

// <1 percent voltage threshold
static const uint32_t Capacitor_empty = 30;
// 20 percent voltage threshold
static const uint32_t V1 = 819;
// 80 percent voltage threshold
static const uint32_t V2 = 3276;
// ln4
static const float ln4 = std::log(4.0f);
// ln0.8
static const float ln0_8 = std::log(0.8f);
// timeout of 6 seconds 
static const uint32_t timeout = 6000;
// minium time of 1 ms
static const float minium_time = 1e-3f;
// minium capacitance readable 
static const float minium_cap = 1e-10f;
// err constant 
static enum cap_meter_error_codes err = cap_ok;


static const pin_struct pins[4] = {
    {GPIOA, GPIO_PIN_9, 1000.0f},
    {GPIOA, GPIO_PIN_12, 10000.0f}, 
    {GPIOB, GPIO_PIN_6, 100000.0f},
    {GPIOA, GPIO_PIN_7, 1000000.0f}

};
static void ground_pins(void){
    for (int i=0; i<4; i++){
        GPIO_InitTypeDef pin = {};
        pin.Pin = pins[i].pin;
        pin.Mode = GPIO_MODE_ANALOG;
        pin.Pull =GPIO_NOPULL;
        HAL_GPIO_Init(pins[i].port, &pin);
    }
}

static uint32_t timeout_cycles(uint32_t time){
    return (SystemCoreClock / 1000) * time;
}

void capacitance_init(void){
    // setup timer 
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    // initalise clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    // switch off resistor pins
    ground_pins();
    // initalise d3
    GPIO_InitTypeDef pin = {};
    pin.Pin = GPIO_PIN_0;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &pin);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
}

// reads ADC value
static uint32_t read_adc(void){
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1);
    uint32_t Adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return Adc_value;
}

static void range_on(int i){
    GPIO_InitTypeDef pin = {};
    pin.Pin = pins[i].pin;
    pin.Mode = GPIO_MODE_OUTPUT_PP; 
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(pins[i].port, pins[i].pin, GPIO_PIN_RESET); 
    HAL_GPIO_Init(pins[i].port, &pin);
}

// discharge capacitor on d2
static bool discharge_capacitor(uint32_t i){
    HAL_GPIO_WritePin(pins[i].port, pins[i].pin, GPIO_PIN_RESET);
    uint32_t start = DWT->CYCCNT;
    while (read_adc() > Capacitor_empty){
        if (DWT->CYCCNT - start > timeout_cycles(timeout)) return false;
    }
    return true;
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

// calculates capacitance from charge time and resitance
static float capacitance_from_time(float dt, float r){
    return dt / (r * ln4);
}

// estimates the capacintance based on charge time to 20 percent 
static uint32_t optimum_resistor(void){
    uint32_t t = 0;
    uint32_t v = 0;
    range_on(1);
    if (!discharge_capacitor(1)) {
        ground_pins();
        err = cap_discharge_timeout;
        return 0; 
    }
    HAL_GPIO_WritePin(pins[1].port, pins[1].pin, GPIO_PIN_SET);
    uint32_t start = DWT->CYCCNT;
    while (true){
        v = read_adc();
        if (v >= V1){
            t = DWT->CYCCNT;
            break;
        }
        if (DWT->CYCCNT - start > timeout_cycles(timeout)){
            ground_pins();
            // timeout picks smallest resistor
            return 0;
        }
    }
    
    discharge_capacitor(1);

    float dt = (float)(t - start) / SystemCoreClock;
    float C = -dt / (pins[1].ohms * ln0_8);
    for (int i=0; i<3; i++){
        const float expected_time = ln4 * pins[i].ohms * C;
        if (expected_time >= minium_time){
            ground_pins();
            return i;
        }
    }
    ground_pins();
    return 3;
}

float capacitance_measure(void){

    // reset error code
    err = cap_ok;

    // initialiase values for t1, t2 and voltage 
    uint32_t t1 = 0, t2 = 0;
    uint32_t v = 0;
    bool t1_ran = false;
    
    select_sense_channel();
    const uint32_t i = optimum_resistor();
    if (err != cap_ok) return err; 
    
    range_on(i);
    if (!discharge_capacitor(i)) {
        ground_pins(); 
        return cap_discharge_timeout;
    }
    
    HAL_GPIO_WritePin(pins[i].port, pins[i].pin, GPIO_PIN_SET);
    
    uint32_t start = DWT->CYCCNT;
    
    while (true){
        v = read_adc();
        if (!t1_ran && v >= V1){
            t1 = DWT->CYCCNT;
            t1_ran = true;
        }
        if (v >= V2){
            t2 = DWT->CYCCNT;
            break;
        }
        if (DWT->CYCCNT - start > timeout_cycles(timeout)) {
            discharge_capacitor(i);
            ground_pins(); 
            return cap_too_large;
        }        
    }
    
    discharge_capacitor(i);
    ground_pins();

    // calculate capacitance 
    float dt = (float)(t2 -t1) / SystemCoreClock;
    float C = capacitance_from_time(dt, pins[i].ohms);
    
    if(C < minium_cap){
        return cap_too_small;
    }
    
    return C;
}
