#ifndef ECHO_CAPACITANCE_H
#define ECHO_CAPACITANCE_H

#include "hardware_manager.h"
#include "measurement.h"
#include <cstdint>

// Owns shared ADC1 channel 1 (PA0/A0) and the four resistor pins while active.
class CapacitanceMeter
{
public:
    enum class Status
    {
        Ok,
        HardwareUnavailable,
        ChannelConfigurationFailed
    };

    // Stores hardware references only; no hardware access during construction.
    explicit CapacitanceMeter(HardwareManager &hardware);
    CapacitanceMeter(const CapacitanceMeter&) = delete;
    CapacitanceMeter& operator=(const CapacitanceMeter&) = delete;
    // Call after HardwareManager::begin().
    Status activate();
    // One blocking attempt: farads, status and an error message.
    // Failed results have a NaN value.
    MeasurementResult read_measurement();
    // Stop excitation and ADC conversions; resistor pins become high impedance.
    void deactivate();

private:
    void release_pins();
    void select_range(uint32_t range);
    bool read_adc(uint32_t &value);
    bool discharge_capacitor(uint32_t range, uint32_t threshold);
    bool charge_measure(uint32_t &range);
    bool discharge_measure(uint32_t range, float &capacitance);

    HardwareManager &hardware_;
    ADC_HandleTypeDef &adc_;
    bool active_ = false;
    MeasurementResult error_; // Reset at the start of every measurement.
};

#endif
