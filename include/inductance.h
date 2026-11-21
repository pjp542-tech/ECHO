#ifndef ECHO_INDUCTANCE_H
#define ECHO_INDUCTANCE_H

#include "hardware_manager.h"
#include "measurement.h"
#include <cstdint>

// Owns TIM2 channel 2, PA1 capture input and PA4 excitation while active.
class InductanceMeter
{
public:
    enum class Status
    {
        Ok,
        HardwareUnavailable,
        ChannelConfigurationFailed,
        TimerStartFailed
    };

    // Store hardware references; construction does not access hardware.
    explicit InductanceMeter(HardwareManager &hardware);
    InductanceMeter(const InductanceMeter&) = delete;
    InductanceMeter& operator=(const InductanceMeter&) = delete;
    // Requires CubeMX to initialise TIM2 as a 10 MHz, 32-bit up-counter.
    Status activate();
    // One blocking attempt: henries, status and an error message.
    MeasurementResult read_measurement();
    // Disconnect excitation, stop capture and release PA1 to high impedance.
    void deactivate();

private:
    void set_pulse_output(bool output);
    bool send_pulse();
    bool wait_for_edge();
    float measure_period();

    HardwareManager &hardware_;
    TIM_HandleTypeDef &timer_;
    bool active_ = false;
    MeasurementResult error_; // Reset at the start of every measurement.
};

#endif
