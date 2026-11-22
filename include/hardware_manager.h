#ifndef ECHO_HARDWARE_MANAGER_H
#define ECHO_HARDWARE_MANAGER_H

#include "stm32g4xx_hal.h"
#include <atomic>

struct Pin {
    GPIO_TypeDef* port;
    uint16_t pin;
};

// NUCLEO-G431KB / MB1430. Keep ECHO.ioc and docs/wiring.md in sync.
namespace BoardPins {
// Common resistor-bank / component junction for both R and C measurements.
inline const Pin rc_sense{GPIOA, GPIO_PIN_0}; // A0, ADC1_IN1
// Shared by resistance and capacitance; only the active meter drives these.
inline const Pin rc_ranges[] = {
    {GPIOA, GPIO_PIN_9},  // D1: 1 kohm
    {GPIOA, GPIO_PIN_12}, // D2: 10 kohm
    {GPIOB, GPIO_PIN_6},  // D6: 100 kohm
    {GPIOA, GPIO_PIN_10}  // D0: 1 Mohm (previously PA7/A6)
};
inline const Pin inductance_capture{GPIOA, GPIO_PIN_1}; // A1, TIM2_CH2 AF1
inline const Pin inductance_pulse{GPIOA, GPIO_PIN_4};   // A3
inline const Pin lcd_scl{GPIOA, GPIO_PIN_15};           // D5 / A5 bridge alias
inline const Pin lcd_sda{GPIOB, GPIO_PIN_7};            // D4 / A4 bridge alias
inline const Pin modebtn{GPIOB, GPIO_PIN_4};              // D12, switch to GND
inline const Pin measurebtn{GPIOB, GPIO_PIN_5};              // D11, switch to GND
inline constexpr uint32_t rc_channel = ADC_CHANNEL_1;
}

class HardwareManager {
public:
    HardwareManager(ADC_HandleTypeDef& adc, TIM_HandleTypeDef& timer,
                    I2C_HandleTypeDef& i2c) : adc_(adc), timer_(timer), i2c_(i2c) {}
    HardwareManager(const HardwareManager&) = delete;
    HardwareManager& operator=(const HardwareManager&) = delete;

    // Once, after CubeMX startup and before any measurement worker starts.
    bool begin();
    bool ready() const { return ready_; }

    // Nonblocking exclusive ownership from activate() through deactivate().
    // This is NOT an RTOS mutex. Contention returns false instead of waiting.
    // Meter objects and ComponentMeter must still be called from one context.
    bool try_acquire(const void* owner);
    void release(const void* owner);

    ADC_HandleTypeDef& adc() { return adc_; }
    TIM_HandleTypeDef& timer() { return timer_; }
    I2C_HandleTypeDef& i2c() { return i2c_; }
    bool configure_adc(Pin sense, uint32_t channel, uint32_t sampling_time);
    static void configure_pin(Pin pin, uint32_t mode, uint32_t alternate = 0);
    static void write(Pin pin, GPIO_PinState state);

private:
    ADC_HandleTypeDef& adc_;
    TIM_HandleTypeDef& timer_;
    I2C_HandleTypeDef& i2c_;
    std::atomic<const void*> owner_{nullptr};
    bool ready_ = false;
};

#endif
