#include "resistance.h"
#include <cstdlib>

namespace
{
// Shared GPIO resistor bank, in ohms. Edit after calibration with the final leads.
constexpr uint32_t reference_ohms[ResistanceMeter::range_count] = {1000, 10000, 100000, 1000000};
static_assert(sizeof(BoardPins::rc_ranges) / sizeof(Pin) == ResistanceMeter::range_count);
constexpr uint32_t adc_full_scale = 4095;
constexpr int adc_midpoint = 2048;
constexpr uint32_t settling_ms = 10;
constexpr uint32_t adc_timeout_ms = 100;
}

ResistanceMeter::ResistanceMeter(HardwareManager &hardware)
    : hardware_(hardware), adc_(hardware.adc())
{
}

// Release every branch before enabling the selected one (break before make).
void ResistanceMeter::select_range(int range)
{
    for (Pin pin : BoardPins::rc_ranges) {
        HardwareManager::configure_pin(pin, GPIO_MODE_ANALOG);
        HardwareManager::write(pin, GPIO_PIN_RESET);
    }
    if (range < 0 || range >= range_count)
        return;
    const Pin selected = BoardPins::rc_ranges[range];
    HardwareManager::write(selected, GPIO_PIN_SET);
    HardwareManager::configure_pin(selected, GPIO_MODE_OUTPUT_PP);
}

ResistanceMeter::Status ResistanceMeter::activate()
{
    if (active_)
        return Status::Ok;
    if (!hardware_.try_acquire(this))
        return Status::HardwareUnavailable;
    select_range(-1); // Leave all branches high impedance until ADC setup succeeds.
    if (!hardware_.configure_adc(BoardPins::resistance_sense,
                                 BoardPins::resistance_channel,
                                 ADC_SAMPLETIME_640CYCLES_5)) {
        hardware_.release(this);
        return Status::ChannelConfigurationFailed;
    }

    active_ = true;
    select_range(0);
    return Status::Ok;
}

MeasurementStatus ResistanceMeter::read_adc(uint32_t &reading)
{
    if (HAL_ADC_Start(&adc_) != HAL_OK)
        return MeasurementStatus::AdcError;
    const auto conversion = HAL_ADC_PollForConversion(&adc_, adc_timeout_ms);
    if (conversion == HAL_OK)
        reading = HAL_ADC_GetValue(&adc_);
    if (HAL_ADC_Stop(&adc_) != HAL_OK)
        return MeasurementStatus::AdcError;
    if (conversion == HAL_TIMEOUT)
        return MeasurementStatus::Timeout;
    return conversion == HAL_OK ? MeasurementStatus::Ok : MeasurementStatus::AdcError;
}

MeasurementResult ResistanceMeter::read_measurement()
{
    if (!active_)
        return MeasurementResult::failure(MeasurementStatus::Inactive, "Resistance inactive");

    MeasurementStatus failure = MeasurementStatus::Timeout;
    int best_range = -1;
    uint32_t best_reading = 0;
    int best_distance = adc_full_scale + 1;
    for (int range = 0; range < range_count; ++range)
    {
        select_range(range);
        HAL_Delay(settling_ms);
        uint32_t reading = 0;
        const auto status = read_adc(reading);
        if (status != MeasurementStatus::Ok) {
            // A hardware error takes priority over timeouts if every range fails.
            if (status == MeasurementStatus::AdcError)
                failure = status;
            continue;
        }
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
        return MeasurementResult::failure(failure, failure == MeasurementStatus::Timeout
            ? "Resistance ADC timeout" : "Resistance ADC failed");
    HAL_Delay(settling_ms);
    if (best_reading >= adc_full_scale)
        return MeasurementResult::failure(MeasurementStatus::OutOfRange, "Resistance ADC saturated");

    // GPIO HIGH -> reference -> shared ADC junction -> unknown -> GND.
    const float effective_reference = static_cast<float>(reference_ohms[best_range]);
    return MeasurementResult::success(
        effective_reference * best_reading / (adc_full_scale - best_reading));
}

void ResistanceMeter::deactivate()
{
    if (!active_)
        return;
    select_range(-1); // Disable excitation before changing any other outputs.
    HAL_ADC_Stop(&adc_);
    active_ = false;
    hardware_.release(this);
}
