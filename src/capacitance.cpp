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

struct RangePin
{
    GPIO_TypeDef *port;
    uint16_t pin;
    float ohms;
};

// Edit resistor calibration values here, in ohms.
const RangePin pins[] = {
    {GPIOA, GPIO_PIN_9, 1000.0f},
    {GPIOA, GPIO_PIN_12, 10000.0f},
    {GPIOB, GPIO_PIN_6, 100000.0f},
    {GPIOA, GPIO_PIN_7, 1000000.0f}
};
constexpr uint32_t range_count = sizeof(pins) / sizeof(pins[0]);

uint32_t timeout_cycles()
{
    return (SystemCoreClock / 1000) * timeout_ms;
}
}

CapacitanceMeter::CapacitanceMeter(ADC_HandleTypeDef &adc) : adc_(adc)
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
    deactivate();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    release_pins();

    GPIO_InitTypeDef sense = {};
    sense.Pin = GPIO_PIN_0;
    sense.Mode = GPIO_MODE_ANALOG;
    sense.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &sense);
    if (HAL_ADCEx_Calibration_Start(&adc_, ADC_SINGLE_ENDED) != HAL_OK)
        return Status::CalibrationFailed;

    ADC_ChannelConfTypeDef channel = {};
    channel.Channel = ADC_CHANNEL_15; // PB0 / board D3.
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    channel.SingleDiff = ADC_SINGLE_ENDED;
    channel.OffsetNumber = ADC_OFFSET_NONE;
    if (HAL_ADC_ConfigChannel(&adc_, &channel) != HAL_OK)
        return Status::ChannelConfigurationFailed;

    // DWT is shared CPU timing hardware. Enable it without resetting the
    // counter; elapsed-time subtraction also handles counter wraparound.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    active_ = true;
    return Status::Ok;
}

bool CapacitanceMeter::read_adc(uint32_t &value)
{
    if (HAL_ADC_Start(&adc_) != HAL_OK)
        return false;
    const bool ready = HAL_ADC_PollForConversion(&adc_, 1) == HAL_OK;
    if (ready)
        value = HAL_ADC_GetValue(&adc_);
    const bool stopped = HAL_ADC_Stop(&adc_) == HAL_OK;
    return ready && stopped;
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
        if (DWT->CYCCNT - start > timeout_cycles())
            return false;
    }
}

bool CapacitanceMeter::charge_measure(uint32_t &range)
{
    select_range(0);
    if (!discharge_capacitor(0, low_threshold - 100))
        return false;

    uint32_t t1 = 0, t2 = 0;
    bool crossed_low = false, crossed_high = false;
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
        if (value >= high_threshold + 100)
            break;
        if (DWT->CYCCNT - start > timeout_cycles())
        {
            discharge_capacitor(0, capacitor_empty);
            return false;
        }
    }

    const float seconds = static_cast<float>(t2 - t1) / SystemCoreClock;
    const float estimate = seconds / (pins[0].ohms * ln4);
    range = range_count - 1;
    for (uint32_t i = 0; i < range_count - 1; ++i)
    {
        if (ln4 * pins[i].ohms * estimate >= minimum_time)
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
            return false;
        }
    }
    const float seconds = static_cast<float>(t1 - t2) / SystemCoreClock;
    capacitance = seconds / (pins[range].ohms * ln4);
    return true;
}

float CapacitanceMeter::read_measurement()
{
    constexpr float invalid = std::numeric_limits<float>::quiet_NaN();
    if (!active_)
        return invalid;

    uint32_t range = 0;
    float capacitance = 0;
    const bool valid = charge_measure(range) && discharge_measure(range, capacitance);
    release_pins(); // Stop excitation on every success, timeout or ADC failure.
    if (!valid || capacitance < minimum_capacitance)
        return invalid;
    return capacitance;
}

void CapacitanceMeter::deactivate()
{
    if (!active_)
        return;
    release_pins();
    HAL_ADC_Stop(&adc_);
    active_ = false;
    // Leave the shared DWT counter running for other meters/debugging.
}
