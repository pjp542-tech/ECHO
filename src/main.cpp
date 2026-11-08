#include "app.h"
#include <cstdlib>

extern "C" {
extern ADC_HandleTypeDef hadc1;
}

const uint32_t known_resistors[4] = {1000, 10000, 100000, 1000000};
const uint32_t mux_resistance = 170; // Estimated ohms; adjust after calibration.
const uint32_t reading_interval_ms = 500;
const uint32_t averaging_duration_ms = 20000;
const bool use_20_second_average = false;
volatile uint32_t adc_raw = 0;
volatile uint32_t resistance_ohms = 0;

// NUCLEO-G431KB: A0 = D12/PB4, A1 = D9/PA8,
// A2 = D10/PA11, EN = D11/PB5. Range 0..3 selects S1..S4.
void select_range(int range)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, (range & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, (range & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
}

void initialise_mux(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOA, &gpio);
    select_range(0);
}

void initialise_adc(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    ADC_ChannelConfTypeDef config = {};
    config.Channel = ADC_CHANNEL_1; // A0 / PA0: divider junction
    config.Rank = ADC_REGULAR_RANK_1;
    config.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
    config.SingleDiff = ADC_SINGLE_ENDED;
    config.OffsetNumber = ADC_OFFSET_NONE;
    HAL_ADC_ConfigChannel(&hadc1, &config);
}

bool read_adc(uint32_t &reading)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
        return false;
    const bool ready = HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK;
    if (ready)
        reading = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return ready;
}

// Exactly one conversion per known resistor; choose closest to half scale.
// Return -1 if no conversion succeeds.
int find_optimal_known_resistor(void)
{
    int best_range = -1;
    int best_distance = 4096;
    for (int range = 0; range < 4; range++)
    {
        select_range(range);
        HAL_Delay(10);
        uint32_t reading = 0;
        if (!read_adc(reading))
            continue;
        const int distance = std::abs((int)reading - 2048);
        if (distance < best_distance)
        {
            best_distance = distance;
            best_range = range;
            adc_raw = reading;
        }
    }
    if (best_range >= 0)
    {
        select_range(best_range);
        HAL_Delay(10); // Settle before subsequent readings on the chosen range.
    }
    return best_range;
}

bool calculate_resistance(int range, uint32_t reading, uint32_t &resistance)
{
    if (range < 0 || range >= 4 || reading >= 4095)
        return false;
    // 3.3 V -> MUX -> known resistor -> ADC junction -> unknown -> GND.
    resistance = (uint32_t)(
        (uint64_t)(known_resistors[range] + mux_resistance) * reading / (4095 - reading));
    return true;
}

// One iteration, called repeatedly by loop(). Recheck the range each time.
void continuous_reading_loop(void)
{
    const int range = find_optimal_known_resistor();
    uint32_t resistance = 0;
    if (calculate_resistance(range, adc_raw, resistance))
    {
        resistance_ohms = resistance;
    }
    HAL_Delay(reading_interval_ms);
}

// Blocking 20-second measurement. Average resistance values, not ADC codes.
// Invalid readings are excluded from the average.
bool read_average_20_seconds(uint32_t &average_ohms)
{
    average_ohms = 0;
    const int range = find_optimal_known_resistor();
    if (range < 0)
    {
        return false;
    }

    uint64_t total = 0;
    uint32_t samples = 0;
    const uint32_t start = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - start) < averaging_duration_ms)
    {
        uint32_t reading = 0;
        uint32_t resistance = 0;
        if (read_adc(reading) && calculate_resistance(range, reading, resistance))
        {
            adc_raw = reading;
            total += resistance;
            samples++;
        }
        const uint32_t elapsed = HAL_GetTick() - start;
        if (elapsed >= averaging_duration_ms)
            break;
        const uint32_t remaining = averaging_duration_ms - elapsed;
        HAL_Delay(remaining < reading_interval_ms ? remaining : reading_interval_ms);
    }
    if (samples == 0)
    {
        return false;
    }
    average_ohms = (uint32_t)(total / samples);
    resistance_ohms = average_ohms;
    return true;
}

void setup(void)
{
    initialise_mux();
    initialise_adc();
}

void loop(void)
{
    if (use_20_second_average)
    {
        uint32_t average = 0;
        read_average_20_seconds(average);
    }
    else
        continuous_reading_loop();
}
