#ifndef ECHO_RESISTANCE_H
#define ECHO_RESISTANCE_H

#include "hardware_manager.h"
#include "measurement.h"
#include <cstdint>

// GPIO-ranged resistance meter on the NUCLEO-G431KB.
// Owns shared ADC1 channel 1 (PA0/A0) and the R/C resistor bank while in use.
class ResistanceMeter
{
public:
    static constexpr int range_count = 4;

    enum class Status
    {
        Ok,
        HardwareUnavailable,
        ChannelConfigurationFailed
    };

    // HardwareManager::begin() must succeed before activate().
    // Construction performs no hardware access. HardwareManager must outlive this object.
    explicit ResistanceMeter(HardwareManager &hardware);
    ResistanceMeter(const ResistanceMeter&) = delete;
    ResistanceMeter& operator=(const ResistanceMeter&) = delete;
    Status activate();
    // Blocking range scan (~50 ms normally); one sample per reference, no averaging.
    // Returns the best scan sample and leaves that range selected.
    // Returns ohms, status and an error message; value is NaN on failure.
    MeasurementResult read_measurement();
    // Release all shared range pins to high impedance and stop ADC conversions.
    // Safe to call repeatedly; inactive calls do not touch shared hardware.
    void deactivate();

private:
    void select_range(int range);
    MeasurementStatus read_adc(uint32_t &reading);

    HardwareManager &hardware_;
    ADC_HandleTypeDef &adc_;
    bool active_ = false;
};

#endif
