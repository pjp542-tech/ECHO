#ifndef ECHO_CAPACITANCE_H
#define ECHO_CAPACITANCE_H

#include "stm32g4xx_hal.h"
#include <cstdint>

// Owns ADC1 channel 15 (PB0) and the four resistor pins while active.
class CapacitanceMeter
{
public:
    enum class Status
    {
        Ok,
        CalibrationFailed,
        ChannelConfigurationFailed
    };

    // Stores the ADC reference only; no hardware access during construction.
    explicit CapacitanceMeter(ADC_HandleTypeDef &adc);
    // Call after CubeMX has initialised the ADC and its clocks.
    Status activate();
    // Blocking measurement in farads. NAN means inactive, ADC failure,
    // timeout or capacitance below the supported minimum.
    float read_measurement();
    // Stop excitation and ADC conversions; resistor pins become high impedance.
    void deactivate();

private:
    void release_pins();
    void select_range(uint32_t range);
    bool read_adc(uint32_t &value);
    bool discharge_capacitor(uint32_t range, uint32_t threshold);
    bool charge_measure(uint32_t &range);
    bool discharge_measure(uint32_t range, float &capacitance);

    ADC_HandleTypeDef &adc_;
    bool active_ = false;
};

#endif
