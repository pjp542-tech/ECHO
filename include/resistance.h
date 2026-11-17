#ifndef ECHO_RESISTANCE_H
#define ECHO_RESISTANCE_H

#include "stm32g4xx_hal.h"
#include <cstdint>

// DG408 resistance meter on the NUCLEO-G431KB.
// Owns ADC1 channel 1 and MUX control pins D9-D12 while in use.
class ResistanceMeter
{
public:
    static constexpr int range_count = 4;

    enum class Status
    {
        Ok,
        CalibrationFailed,
        ChannelConfigurationFailed
    };

    // ADC and its clocks must already be configured by CubeMX before activate().
    // The constructor performs no hardware access. The ADC must outlive this object.
    explicit ResistanceMeter(ADC_HandleTypeDef &adc);
    Status activate();
    // Blocking range scan (~50 ms normally); one sample per reference, no averaging.
    // Returns the best scan sample and leaves that range selected.
    // Returns resistance in ohms, or NAN if inactive, all reads fail or ADC saturates.
    float read_measurement();
    // Disable excitation, stop ADC conversions and set MUX address outputs LOW.
    // Safe to call repeatedly; inactive calls do not touch shared hardware.
    void deactivate();

private:
    void initialise_mux();
    void select_range(int range);
    bool read_adc(uint32_t &reading);

    ADC_HandleTypeDef &adc_;
    bool active_ = false;
};

#endif
