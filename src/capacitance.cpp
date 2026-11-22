#include "capacitance.h"
#include <limits>

namespace
{
constexpr uint32_t capacitor_empty = 30;
constexpr uint32_t low_threshold = 819;   // 20% of 12-bit full scale.
constexpr uint32_t high_threshold = 3276; // 80% of 12-bit full scale.
constexpr float ln4 = 1.38629436112f;
constexpr uint32_t timeout_ms = 8000;
constexpr float minimum_time = 1e-3f;
constexpr float minimum_capacitance = 1e-10f;

// Calibration stays with the algorithm; physical pins live in HardwareManager.
constexpr float reference_ohms[] = {1000.0f, 10000.0f, 100000.0f, 1000000.0f};
const auto &pins = BoardPins::rc_ranges;
constexpr uint32_t range_count = sizeof(reference_ohms) / sizeof(reference_ohms[0]);
static_assert(range_count == sizeof(BoardPins::rc_ranges) / sizeof(Pin));

uint32_t timeout_cycles()
{
    return (SystemCoreClock / 1000) * timeout_ms;
}
}

CapacitanceMeter::CapacitanceMeter(HardwareManager &hardware)
    : hardware_(hardware), adc_(hardware.adc())
{
}

void CapacitanceMeter::release_pins()
{
    for (const auto &range : pins)
    {
        GPIO_InitTypeDef pin = {};
        pin.Pin = range.pin;
        pin.Mode = GPIO_MODE_ANALOG;
        pin.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(range.port, &pin);
        HAL_GPIO_WritePin(range.port, range.pin, GPIO_PIN_RESET);
    }
}

void CapacitanceMeter::select_range(uint32_t range)
{
    release_pins();
    GPIO_InitTypeDef pin = {};
    pin.Pin = pins[range].pin;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(pins[range].port, pins[range].pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(pins[range].port, &pin);
}

CapacitanceMeter::Status CapacitanceMeter::activate()
{
    if (active_)
        return Status::Ok;
    if (!hardware_.try_acquire(this))
        return Status::HardwareUnavailable;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    release_pins();

    if (!hardware_.configure_adc(BoardPins::rc_sense,
                                 BoardPins::rc_channel,
                                 ADC_SAMPLETIME_47CYCLES_5)) {
        hardware_.release(this);
        return Status::ChannelConfigurationFailed;
    }

    active_ = true;
    return Status::Ok;
}

bool CapacitanceMeter::read_adc(uint32_t &value)
{
    if (HAL_ADC_Start(&adc_) != HAL_OK) {
        error_ = MeasurementResult::failure(MeasurementStatus::AdcError, "Capacitance ADC start failed");
        return false;
    }
    const auto conversion = HAL_ADC_PollForConversion(&adc_, 1);
    if (conversion == HAL_OK)
        value = HAL_ADC_GetValue(&adc_);
    if (HAL_ADC_Stop(&adc_) != HAL_OK) {
        error_ = MeasurementResult::failure(MeasurementStatus::AdcError, "Capacitance ADC stop failed");
        return false;
    }
    if (conversion != HAL_OK) {
        error_ = MeasurementResult::failure(
            conversion == HAL_TIMEOUT ? MeasurementStatus::Timeout : MeasurementStatus::AdcError,
            conversion == HAL_TIMEOUT ? "Capacitance ADC timeout" : "Capacitance ADC failed");
        return false;
    }
    return true;
}

bool CapacitanceMeter::discharge_capacitor(uint32_t range, uint32_t threshold)
{
    HAL_GPIO_WritePin(pins[range].port, pins[range].pin, GPIO_PIN_RESET);
    const uint32_t start = DWT->CYCCNT;
    while (true)
    {
        uint32_t value = 0;
        if (!read_adc(value))
            return false;
        if (value < threshold)
            return true;
        if (DWT->CYCCNT - start > timeout_cycles()) {
            error_ = MeasurementResult::failure(MeasurementStatus::Timeout, "Capacitor discharge timeout");
            return false;
        }
    }
}

bool CapacitanceMeter::charge_measure(uint32_t &range)
{
    select_range(0);
    if (!discharge_capacitor(0, low_threshold - 100))
        return false;

    uint32_t t1 = 0, t2 = 0;
    bool crossed_low = false, crossed_high = false;
    constexpr uint32_t charge_target = high_threshold + 100;
    const uint32_t start = DWT->CYCCNT;
    HAL_GPIO_WritePin(pins[0].port, pins[0].pin, GPIO_PIN_SET);
    while (true)
    {
        uint32_t value = 0;
        if (!read_adc(value))
            return false;
        if (!crossed_low && value >= low_threshold)
        {
            t1 = DWT->CYCCNT;
            crossed_low = true;
        }
        if (!crossed_high && value >= high_threshold)
        {
            t2 = DWT->CYCCNT;
            crossed_high = true;
        }
        if (value >= charge_target)
            break;
        if (DWT->CYCCNT - start > timeout_cycles())
        {
            discharge_capacitor(0, capacitor_empty);
            error_ = MeasurementResult::failure(MeasurementStatus::Timeout, "Capacitor charge timeout");
            return false;
        }
    }

    const float seconds = static_cast<float>(t2 - t1) / SystemCoreClock;
    const float estimate = seconds / (reference_ohms[0] * ln4);
    range = range_count - 1;
    for (uint32_t i = 0; i < range_count - 1; ++i)
    {
        if (ln4 * reference_ohms[i] * estimate >= minimum_time)
        {
            range = i;
            break;
        }
    }
    release_pins();
    return true;
}

bool CapacitanceMeter::discharge_measure(uint32_t range, float &capacitance)
{
    uint32_t t1 = 0, t2 = 0;
    bool crossed_high = false;
    select_range(range); // Start discharging through the selected resistor.
    const uint32_t start = DWT->CYCCNT;
    while (true)
    {
        uint32_t value = 0;
        if (!read_adc(value))
            return false;
        if (!crossed_high && value <= high_threshold)
        {
            t2 = DWT->CYCCNT;
            crossed_high = true;
        }
        if (value <= low_threshold)
        {
            t1 = DWT->CYCCNT;
            break;
        }
        if (DWT->CYCCNT - start > timeout_cycles())
        {
            discharge_capacitor(range, capacitor_empty);
            error_ = MeasurementResult::failure(MeasurementStatus::Timeout, "Capacitance timing timeout");
            return false;
        }
    }
    const float seconds = static_cast<float>(t1 - t2) / SystemCoreClock;
    capacitance = seconds / (reference_ohms[range] * ln4);
    return true;
}

MeasurementResult CapacitanceMeter::read_measurement()
{
    error_ = {};
    if (!active_)
        return MeasurementResult::failure(MeasurementStatus::Inactive, "Capacitance inactive");

    uint32_t range = 0;
    float capacitance = 0;
    const bool valid = charge_measure(range) && discharge_measure(range, capacitance);
    release_pins(); // Stop excitation on every success, timeout or ADC failure.
    if (!valid)
        return error_;
    if (capacitance < minimum_capacitance)
        return MeasurementResult::failure(MeasurementStatus::OutOfRange, "Capacitance below minimum");
    return MeasurementResult::success(capacitance);
}

void CapacitanceMeter::deactivate()
{
    if (!active_)
        return;
    release_pins();
    HAL_ADC_Stop(&adc_);
    active_ = false;
    hardware_.release(this);
    // Leave the shared DWT counter running for other meters/debugging.
}
