#include "inductance.h"
#include <limits>

namespace
{
// Circuit and timing calibration: edit here rather than in main.cpp.
constexpr float capacitor_farads = 10e-9f;
constexpr float period_correction = 1.1f; // Preserve the existing +10% correction.
constexpr float ticks_per_microsecond = 10.0f;
constexpr float pi = 3.1415927f;
constexpr uint32_t pulse_ticks = 20; // 2 microseconds at 10 MHz.
constexpr uint32_t edge_timeout_ms = 5;
constexpr float minimum_period_us = 5.0f;
constexpr float maximum_period_us = 100.0f;
constexpr float invalid = std::numeric_limits<float>::quiet_NaN();
}

InductanceMeter::InductanceMeter(TIM_HandleTypeDef &timer) : timer_(timer)
{
}

void InductanceMeter::set_pulse_output(bool output)
{
    GPIO_InitTypeDef pin = {};
    pin.Pin = GPIO_PIN_4;
    pin.Mode = output ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &pin);
}

InductanceMeter::Status InductanceMeter::activate()
{
    deactivate();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    set_pulse_output(false);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

    // Restore the capture pin after another meter has used it.
    GPIO_InitTypeDef pin = {};
    pin.Pin = GPIO_PIN_1;
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    pin.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &pin);

    TIM_IC_InitTypeDef capture = {};
    capture.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    capture.ICSelection = TIM_ICSELECTION_DIRECTTI;
    capture.ICPrescaler = TIM_ICPSC_DIV1;
    capture.ICFilter = 0;
    const bool configured =
        HAL_TIM_IC_ConfigChannel(&timer_, &capture, TIM_CHANNEL_2) == HAL_OK;
    if (!configured || HAL_TIM_IC_Start(&timer_, TIM_CHANNEL_2) != HAL_OK)
    {
        HAL_TIM_IC_Stop(&timer_, TIM_CHANNEL_2);
        pin.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(GPIOA, &pin);
        return configured ? Status::TimerStartFailed : Status::ChannelConfigurationFailed;
    }
    __HAL_TIM_CLEAR_FLAG(&timer_, TIM_FLAG_CC2 | TIM_FLAG_CC2OF);
    active_ = true;
    HAL_Delay(20);
    return Status::Ok;
}

bool InductanceMeter::send_pulse()
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    set_pulse_output(true);
    const uint32_t start = __HAL_TIM_GET_COUNTER(&timer_);
    const uint32_t watchdog_start = HAL_GetTick();
    while (static_cast<uint32_t>(__HAL_TIM_GET_COUNTER(&timer_) - start) < pulse_ticks)
    {
        // Release excitation even if the timer unexpectedly stops.
        if (HAL_GetTick() - watchdog_start >= 1)
        {
            set_pulse_output(false);
            return false;
        }
    }
    set_pulse_output(false);
    return true;
}

bool InductanceMeter::wait_for_edge()
{
    const uint32_t start = HAL_GetTick();
    while (__HAL_TIM_GET_FLAG(&timer_, TIM_FLAG_CC2) == RESET)
    {
        if (HAL_GetTick() - start >= edge_timeout_ms)
            return false;
    }
    return true;
}

float InductanceMeter::measure_period()
{
    __HAL_TIM_CLEAR_FLAG(&timer_, TIM_FLAG_CC2 | TIM_FLAG_CC2OF);
    if (!wait_for_edge())
        return invalid;
    // Reading CCR2 clears CC2IF; CC2OF indicates a lost capture.
    const uint32_t first = HAL_TIM_ReadCapturedValue(&timer_, TIM_CHANNEL_2);
    if (__HAL_TIM_GET_FLAG(&timer_, TIM_FLAG_CC2OF) != RESET)
        return invalid;
    if (!wait_for_edge())
        return invalid;
    const uint32_t second = HAL_TIM_ReadCapturedValue(&timer_, TIM_CHANNEL_2);
    if (__HAL_TIM_GET_FLAG(&timer_, TIM_FLAG_CC2OF) != RESET)
        return invalid;
    const uint32_t elapsed = second - first; // Handles 32-bit counter wrap.
    return elapsed / ticks_per_microsecond;
}

float InductanceMeter::read_measurement()
{
    if (!active_ || !send_pulse())
        return invalid;
    const float period_us = measure_period() * period_correction;
    // This also rejects NAN from a failed capture.
    if (!(period_us >= minimum_period_us && period_us <= maximum_period_us))
        return invalid;
    const float frequency_hz = 1000000.0f / period_us;
    return 1.0f / (4.0f * pi * pi * frequency_hz * frequency_hz * capacitor_farads);
}

void InductanceMeter::deactivate()
{
    if (!active_)
        return;
    set_pulse_output(false);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_TIM_IC_Stop(&timer_, TIM_CHANNEL_2);
    __HAL_TIM_CLEAR_FLAG(&timer_, TIM_FLAG_CC2 | TIM_FLAG_CC2OF);
    GPIO_InitTypeDef pin = {};
    pin.Pin = GPIO_PIN_1;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &pin);
    active_ = false;
}
