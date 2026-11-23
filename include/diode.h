#ifndef ECHO_DIODE_H
#define ECHO_DIODE_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

// Bidirectional diode tester on NUCLEO-G431KB.
// Owns ADC1 channels 1 (PA0) and 2 (PA1), plus drive pins PA5 (A4) and PB3 (D13) while active.
class DiodeMeter
{
public:
    enum class Status
    {
        Ok,
        CalibrationFailed,
        ChannelConfigurationFailed
    };

    enum class ResultState
    {
        GoodForward,  // Anode = Terminal A, Cathode = Terminal B
        GoodReverse,  // Anode = Terminal B, Cathode = Terminal A
        OpenCircuit,  // No diode connected or failed open
        ShortCircuit, // Shorted diode or wire link
        FaultyLeaky,  // Conducts in both directions (resistor or damaged junction)
        Inactive,
        ReadError
    };

    struct Measurement
    {
        ResultState state = ResultState::Inactive;
        float forward_voltage_v = 0.0f;
        float forward_current_ma = 0.0f;
    };

    // ADC and its clocks must already be configured by CubeMX before activate().
    // The constructor performs no hardware access. The ADC must outlive this object.
    explicit DiodeMeter(ADC_HandleTypeDef &adc);

    Status activate();

    // Runs forward (A -> B) and reverse (B -> A) bias tests and classifies the component.
    Measurement read_measurement();

    // Sets drive pins to high-impedance analog mode and stops ADC conversions.
    // Safe to call repeatedly; prevents interference with ResistanceMeter on PA0.
    void deactivate();

private:
    void set_drive_pins_output();
    void set_drive_pins_hiz();
    bool read_adc_channel_volts(uint32_t channel, float &volts);
    bool measure_direction(bool drive_a_high, float &v_drop);

    ADC_HandleTypeDef &adc_;
    bool active_ = false;
};

#endif