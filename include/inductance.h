#ifndef ECHO_INDUCTANCE_H
#define ECHO_INDUCTANCE_H

#include "stm32g4xx_hal.h"
#include <cstdint>

// Owns TIM2 channel 2, PA1 capture input and PA4 excitation while active.
class InductanceMeter
{
public:
    enum class Status
    {
        Ok,
        ChannelConfigurationFailed,
        TimerStartFailed
    };

    // Store the timer reference; construction does not access hardware.
    explicit InductanceMeter(TIM_HandleTypeDef &timer);
    // Requires CubeMX to initialise TIM2 as a 10 MHz, 32-bit up-counter.
    Status activate();
    // One blocking attempt. Returns henries, or NAN if inactive/invalid.
    float read_measurement();
    // Disconnect excitation, stop capture and release PA1 to high impedance.
    void deactivate();

private:
    void set_pulse_output(bool output);
    bool send_pulse();
    bool wait_for_edge();
    float measure_period();

    TIM_HandleTypeDef &timer_;
    bool active_ = false;
};

#endif
