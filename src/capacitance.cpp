#include "capacitance.h"
#include "app.h"

#include <cmath>

// shared adc handle
static ADC_HandleTypeDef *adc = nullptr;

// <1 percent voltage threshold
static const uint32_t Capacitor_empty = 30;
// 20 percent voltage threshold
static const uint32_t V1 = 819;
// 80 percent voltage threshold
static const uint32_t V2 = 3276;
// ln4
static const float ln4 = std::log(4.0f);
// timeout of 8 seconds 
static const uint32_t timeout = 8000;
// minimum time of 1 ms
static const float minimum_time = 1e-3f;
// minimum capacitance readable 
static const float minimum_cap = 1e-10f;
// err 
static enum cap_meter_error_codes err = cap_ok;

typedef struct{
    GPIO_TypeDef *port;
    uint32_t pin;
    float ohms;
} pin_struct;

static const pin_struct pins[4] = {
    {GPIOA, GPIO_PIN_9, 1000.0f},
    {GPIOA, GPIO_PIN_12, 10000.0f}, 
    {GPIOB, GPIO_PIN_6, 100000.0f},
    {GPIOA, GPIO_PIN_10, 1000000.0f} 
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

// reads ADC value, sets err if the ADC fails
static uint32_t read_adc(void){
    uint32_t Adc_value = 0;
    if (HAL_ADC_Start(adc) != HAL_OK){
        err = cap_adc_error;
        return 0;
    }
    if (HAL_ADC_PollForConversion(adc, 1) == HAL_OK){
        Adc_value = HAL_ADC_GetValue(adc);
    }
    else{
        err = cap_adc_error;
    }
    HAL_ADC_Stop(adc);
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

static bool discharge_capacitor(uint32_t i, uint32_t v){
    HAL_GPIO_WritePin(pins[i].port, pins[i].pin, GPIO_PIN_RESET);
    uint32_t start = DWT->CYCCNT;
    while (read_adc() >= v){
        if (err != cap_ok){
            return false;
        }
        if (DWT->CYCCNT - start > timeout_cycles(timeout)) return false;
    }
    if (err != cap_ok){
        return false;
    }
    return true;
}

// sets up shared adc read channel
static void select_sense_channel(void){
    ADC_ChannelConfTypeDef ch = {};
    ch.Channel = BoardPins::rc_channel;
    ch.Rank = ADC_REGULAR_RANK_1;
    // Allow more acquisition time for the ADC input to settle.
    ch.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    ch.SingleDiff = ADC_SINGLE_ENDED;
    ch.OffsetNumber = ADC_OFFSET_NONE;
    if (HAL_ADC_ConfigChannel(adc, &ch) != HAL_OK) err = cap_channel_config_failed;
}

static float capacitance_from_time(float dt, float r){
    return dt / (r * ln4);
}

// estimates the capacitance based on charge time from 20 to 80 percent using the smallest resistor  
static uint32_t charge_measure(void){
    uint32_t t1 = 0, t2 =0;
    bool t1_ran = false, t2_ran = false;
    uint32_t v = 0;
    uint32_t optimum_resistor = 3; // defaults to the largest resistor 
    
    range_on(0);
    
    if (!discharge_capacitor(0, V1 - 100)) {
        ground_pins();
        // keep adc error if there was one
        if (err == cap_ok){
            err = cap_too_large; // timeout on smallest resistor means its too large
        }
        return 0; 
    }
    
    uint32_t start = DWT->CYCCNT;
    
    HAL_GPIO_WritePin(pins[0].port, pins[0].pin, GPIO_PIN_SET);
    
    while (true){
        v = read_adc();
        if (err != cap_ok){
            ground_pins();
            return 0;
        }
        if (!t1_ran && v >= V1){
            t1 = DWT->CYCCNT;
            t1_ran = true;
        }
        if (!t2_ran && v >= V2){
            t2 = DWT->CYCCNT;
            t2_ran = true;
        }
        if (v >= V2 +100){
            break;
        }
        if (DWT->CYCCNT - start > timeout_cycles(timeout)) {
            discharge_capacitor(0, Capacitor_empty);
            ground_pins(); 
            err = cap_too_large; // timeout on smallest resistor means its too large 
            return 0;
        }        
    }   
    
    // calculate capacitance 
    float dt = (float)(t2 -t1) / SystemCoreClock;
    float C = capacitance_from_time(dt, pins[0].ohms);
    
    for (int i=0; i<3; i++){
        const float expected_time = ln4 * pins[i].ohms * C;
        if (expected_time >= minimum_time){
            optimum_resistor = i;
            break;
        }
    }
    ground_pins();
    return optimum_resistor;
}

static float discharge_measure(uint32_t optimum_resistor){
    uint32_t t1 = 0, t2 =0;
    uint32_t v = 0;
    bool t2_ran = false;
    range_on(optimum_resistor);
    
    uint32_t start = DWT->CYCCNT;
    
    HAL_GPIO_WritePin(pins[optimum_resistor].port, pins[optimum_resistor].pin, GPIO_PIN_RESET);
    
    while (true){
        v = read_adc();
        if (err != cap_ok){
            ground_pins();
            return 0;
        }
        if (!t2_ran && v <= V2){
            t2 = DWT->CYCCNT;
            t2_ran = true;
        }
        if (v <= V1){
            t1 = DWT->CYCCNT;
            break;
        }
        if (DWT->CYCCNT - start > timeout_cycles(timeout)) {
            discharge_capacitor(optimum_resistor, Capacitor_empty);
            ground_pins(); 
            err = cap_too_large; 
            return 0;
        }        
    }   
    
    // calculate capacitance 
    float dt = (float)(t1 - t2) / SystemCoreClock;
    float C = capacitance_from_time(dt, pins[optimum_resistor].ohms);
    
    ground_pins();
    return C;
}

static cap_meter_error_codes capacitance_measure(float &C){
    
    // reset error code
    err = cap_ok;
    C = 0;
    if (adc == nullptr) return cap_not_initialised;
    
    select_sense_channel();
    if (err != cap_ok) return err; 
    
    uint32_t optimum_resistor = charge_measure();
    if (err != cap_ok) return err; 
    C = discharge_measure(optimum_resistor);
    if (err != cap_ok) return err; 
    
    if(C < minimum_cap){
        return cap_too_small;
    }
    
    return cap_ok;
}

CapacitanceMeter::CapacitanceMeter(HardwareManager &hardware) : hardware_(hardware){
}

CapacitanceMeter::Status CapacitanceMeter::activate(){
    if (active_) return Status::Ok;
    if (!hardware_.try_acquire(this)) return Status::HardwareUnavailable;
    adc = &hardware_.adc();
    ground_pins();
    err = cap_ok;
    select_sense_channel();
    if (err != cap_ok){
        adc = nullptr;
        hardware_.release(this);
        return Status::ChannelConfigurationFailed;
    }
    active_ = true;
    return Status::Ok;
}

MeasurementResult CapacitanceMeter::read_measurement(){
    if (!active_){
        return MeasurementResult::failure(MeasurementStatus::Inactive, "Capacitance inactive");
    }

    float C = 0;
    cap_meter_error_codes result = capacitance_measure(C);

    // turn error code into status and message for the lcd
    if (result == cap_ok){
        return MeasurementResult::success(C);
    }
    else if (result == cap_too_large){
        return MeasurementResult::failure(MeasurementStatus::OutOfRange, "Capacitance too large");
    }
    else if (result == cap_too_small){
        return MeasurementResult::failure(MeasurementStatus::OutOfRange, "Capacitance too small");
    }
    else if (result == cap_adc_error){
        return MeasurementResult::failure(MeasurementStatus::AdcError, "Capacitance ADC failed");
    }
    else if (result == cap_channel_config_failed){
        return MeasurementResult::failure(MeasurementStatus::AdcError, "Capacitance ADC setup failed");
    }
    else{
        return MeasurementResult::failure(MeasurementStatus::Inactive, "Capacitance inactive");
    }
}

void CapacitanceMeter::deactivate(){
    if (!active_) return;
    ground_pins();
    HAL_ADC_Stop(adc);
    adc = nullptr;
    active_ = false;
    hardware_.release(this);
}
