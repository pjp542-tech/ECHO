#include "resistance.h"
#include <cstdlib>
#include <limits>

namespace
{
// Calibration values for S1-S4, in ohms. Edit these manually after calibration.
constexpr uint32_t reference_ohms[ResistanceMeter::range_count] = {1000, 10000, 100000, 1000000};
// Set to zero if the values above already include the MUX resistance.
constexpr uint32_t mux_ohms = 170; // Existing estimate until calibrated.
constexpr uint32_t adc_full_scale = 4095;
constexpr int adc_midpoint = 2048;
constexpr uint32_t settling_ms = 10;
constexpr uint32_t adc_timeout_ms = 100;
}

ResistanceMeter::ResistanceMeter(ADC_HandleTypeDef &adc)
    : adc_(adc)
{
}

// Address A0=D12/PB4, A1=D9/PA8, A2=D10/PA11; active-HIGH EN=D11/PB5.
void ResistanceMeter::select_range(int range)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
    if (range < 0 || range >= range_count)
        return;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, (range & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, (range & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
}

void ResistanceMeter::initialise_mux()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_11, GPIO_PIN_RESET);
    GPIO_InitTypeDef gpio = {};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOA, &gpio);
}

ResistanceMeter::Status ResistanceMeter::activate()
{
    deactivate();
    initialise_mux(); // Leave disabled until ADC setup succeeds.
    if (HAL_ADCEx_Calibration_Start(&adc_, ADC_SINGLE_ENDED) != HAL_OK)
        return Status::CalibrationFailed;

    // Restore the sense pin when returning from another meter.
    GPIO_InitTypeDef sense = {};
    sense.Pin = GPIO_PIN_0;
    sense.Mode = GPIO_MODE_ANALOG;
    sense.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &sense);

    ADC_ChannelConfTypeDef channel = {};
    channel.Channel = ADC_CHANNEL_1; // Board A0 / PA0, not MUX address A0.
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
    channel.SingleDiff = ADC_SINGLE_ENDED;
    channel.OffsetNumber = ADC_OFFSET_NONE;
    if (HAL_ADC_ConfigChannel(&adc_, &channel) != HAL_OK)
        return Status::ChannelConfigurationFailed;

    active_ = true;
    select_range(0);
    return Status::Ok;
}

bool ResistanceMeter::read_adc(uint32_t &reading)
{
    if (HAL_ADC_Start(&adc_) != HAL_OK)
        return false;
    const bool ready = HAL_ADC_PollForConversion(&adc_, adc_timeout_ms) == HAL_OK;
    if (ready)
        reading = HAL_ADC_GetValue(&adc_);
    const bool stopped = HAL_ADC_Stop(&adc_) == HAL_OK;
    return ready && stopped;
}

float ResistanceMeter::read_measurement()
{
    constexpr float invalid = std::numeric_limits<float>::quiet_NaN();
    if (!active_)
        return invalid;

    int best_range = -1;
    uint32_t best_reading = 0;
    int best_distance = adc_full_scale + 1;
    for (int range = 0; range < range_count; ++range)
    {
        select_range(range);
        HAL_Delay(settling_ms);
        uint32_t reading = 0;
        if (!read_adc(reading))
            continue;
        const int distance = std::abs(static_cast<int>(reading) - adc_midpoint);
        if (distance < best_distance)
        {
            best_distance = distance;
            best_range = range;
            best_reading = reading;
        }
    }

    select_range(best_range);
    if (best_range < 0)
        return invalid;
    HAL_Delay(settling_ms);
    if (best_reading >= adc_full_scale)
        return invalid;

    // 3V3 -> MUX -> reference -> ADC junction -> unknown -> GND.
    const float effective_reference = static_cast<float>(reference_ohms[best_range]) + mux_ohms;
    return effective_reference * best_reading / (adc_full_scale - best_reading);
}

void ResistanceMeter::deactivate()
{
    if (!active_)
        return;
    select_range(-1); // Disable excitation before changing any other outputs.
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_ADC_Stop(&adc_);
    active_ = false;
}
