#include "diode.h"
#include <cmath>

namespace  
{
// Hardware calibration constants. Measure your board 3V3 rail and resistors to refine accuracy.
constexpr float vcc_volts = 3.30f;
constexpr float r1_ohms = 1000.0f;
constexpr float r2_ohms = 1000.0f;
constexpr float total_series_ohms = r1_ohms + r2_ohms;

constexpr float adc_full_scale = 4095.0f;
constexpr uint32_t adc_timeout_ms = 100;
constexpr uint32_t settling_ms = 5;
constexpr int sample_count = 8;

// Classification thresholds in volts
constexpr float short_threshold_v = 0.10f;
constexpr float open_threshold_v = 2.95f;

// Drive pins: Drive A = PA5 (A4), Drive B = PB3 (D13)
constexpr uint16_t drive_a_pin = GPIO_PIN_5; // GPIOA
constexpr uint16_t drive_b_pin = GPIO_PIN_3; // GPIOB

// Sense pins: Sense A = PA1 (A1 / ADC1_IN2), Sense B = PA0 (A0 / ADC1_IN1)
constexpr uint16_t sense_a_pin = GPIO_PIN_1; // GPIOA
constexpr uint16_t sense_b_pin = GPIO_PIN_0; // GPIOA
} // namespace

DiodeMeter::DiodeMeter(ADC_HandleTypeDef &adc)
: adc_(adc)
{
}

void DiodeMeter::set_drive_pins_output()
{
HAL_GPIO_WritePin(GPIOA, drive_a_pin, GPIO_PIN_RESET);
HAL_GPIO_WritePin(GPIOB, drive_b_pin, GPIO_PIN_RESET);

GPIO_InitTypeDef gpio = {};
gpio.Mode = GPIO_MODE_OUTPUT_PP;
gpio.Pull = GPIO_NOPULL;
gpio.Speed = GPIO_SPEED_FREQ_LOW;

gpio.Pin = drive_a_pin;
HAL_GPIO_Init(GPIOA, &gpio);

gpio.Pin = drive_b_pin;
HAL_GPIO_Init(GPIOB, &gpio);
}

void DiodeMeter::set_drive_pins_hiz()
{
GPIO_InitTypeDef gpio = {};
gpio.Mode = GPIO_MODE_ANALOG;
gpio.Pull = GPIO_NOPULL;

gpio.Pin = drive_a_pin;
HAL_GPIO_Init(GPIOA, &gpio);

gpio.Pin = drive_b_pin;
HAL_GPIO_Init(GPIOB, &gpio);
}

DiodeMeter::Status DiodeMeter::activate()
{
deactivate();

__HAL_RCC_GPIOA_CLK_ENABLE();
__HAL_RCC_GPIOB_CLK_ENABLE();

// Configure sense pins PA1 (ADC1_IN2) and PA0 (ADC1_IN1) as analog inputs.
GPIO_InitTypeDef sense = {};
sense.Mode = GPIO_MODE_ANALOG;
sense.Pull = GPIO_NOPULL;

sense.Pin = sense_a_pin;
HAL_GPIO_Init(GPIOA, &sense);

sense.Pin = sense_b_pin;
HAL_GPIO_Init(GPIOA, &sense);

set_drive_pins_output();

if (HAL_ADCEx_Calibration_Start(&adc_, ADC_SINGLE_ENDED) != HAL_OK)
return Status::CalibrationFailed;

active_ = true;
return Status::Ok;
}

bool DiodeMeter::read_adc_channel_volts(uint32_t channel, float &volts)
{
ADC_ChannelConfTypeDef config = {};
config.Channel = channel;
config.Rank = ADC_REGULAR_RANK_1;
config.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
config.SingleDiff = ADC_SINGLE_ENDED;
config.OffsetNumber = ADC_OFFSET_NONE;

if (HAL_ADC_ConfigChannel(&adc_, &config) != HAL_OK)
return false;

uint32_t sum = 0;
for (int i = 0; i < sample_count; ++i)
{
if (HAL_ADC_Start(&adc_) != HAL_OK)
return false;
const bool ready = (HAL_ADC_PollForConversion(&adc_, adc_timeout_ms) == HAL_OK);
if (ready)
sum += HAL_ADC_GetValue(&adc_);
const bool stopped = (HAL_ADC_Stop(&adc_) == HAL_OK);
if (!ready || !stopped)
return false;
}

const float avg_counts = static_cast<float>(sum) / static_cast<float>(sample_count);
volts = (avg_counts / adc_full_scale) * vcc_volts;
return true;
}

bool DiodeMeter::measure_direction(bool drive_a_high, float &v_drop)
{
HAL_GPIO_WritePin(GPIOA, drive_a_pin, drive_a_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
HAL_GPIO_WritePin(GPIOB, drive_b_pin, drive_a_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
HAL_Delay(settling_ms);

float va = 0.0f;
float vb = 0.0f;
const bool ok_a = read_adc_channel_volts(ADC_CHANNEL_2, va); // PA1 / Terminal A
const bool ok_b = read_adc_channel_volts(ADC_CHANNEL_1, vb); // PA0 / Terminal B

// Return both drive pins LOW between measurements to avoid continuous load current.
HAL_GPIO_WritePin(GPIOA, drive_a_pin, GPIO_PIN_RESET);
HAL_GPIO_WritePin(GPIOB, drive_b_pin, GPIO_PIN_RESET);

if (!ok_a || !ok_b)
return false;

v_drop = drive_a_high ? (va - vb) : (vb - va);
if (v_drop < 0.0f)
v_drop = 0.0f;
return true;
}

DiodeMeter::Measurement DiodeMeter::read_measurement()
{
Measurement result;
if (!active_)
{
result.state = ResultState::Inactive;
return result;
}

float v_ab = 0.0f;
float v_ba = 0.0f;
if (!measure_direction(true, v_ab) || !measure_direction(false, v_ba))
{
result.state = ResultState::ReadError;
return result;
}

const bool conducts_ab = (v_ab >= short_threshold_v && v_ab <= open_threshold_v);
const bool conducts_ba = (v_ba >= short_threshold_v && v_ba <= open_threshold_v);
const bool short_ab = (v_ab < short_threshold_v);
const bool short_ba = (v_ba < short_threshold_v);
const bool open_ab = (v_ab > open_threshold_v);
const bool open_ba = (v_ba > open_threshold_v);

if (short_ab && short_ba)
{
result.state = ResultState::ShortCircuit;
}
else if (open_ab && open_ba)
{
result.state = ResultState::OpenCircuit;
}
else if (conducts_ab && open_ba)
{
result.state = ResultState::GoodForward;
result.forward_voltage_v = v_ab;
result.forward_current_ma = ((vcc_volts - v_ab) / total_series_ohms) * 1000.0f;
}
else if (conducts_ba && open_ab)
{
result.state = ResultState::GoodReverse;
result.forward_voltage_v = v_ba;
result.forward_current_ma = ((vcc_volts - v_ba) / total_series_ohms) * 1000.0f;
}
else
{
result.state = ResultState::FaultyLeaky;
result.forward_voltage_v = (v_ab < v_ba) ? v_ab : v_ba;
}

return result;
}

void DiodeMeter::deactivate()
{
if (!active_)
return;
HAL_ADC_Stop(&adc_);
set_drive_pins_hiz();
active_ = false;
}