#include "hardware_manager.h"

void HardwareManager::configure_pin(Pin pin, uint32_t mode, uint32_t alternate)
{
    GPIO_InitTypeDef config = {};
    config.Pin = pin.pin;
    config.Mode = mode;
    config.Pull = GPIO_NOPULL;
    config.Speed = GPIO_SPEED_FREQ_LOW;
    config.Alternate = alternate;
    HAL_GPIO_Init(pin.port, &config);
}

void HardwareManager::write(Pin pin, GPIO_PinState state)
{
    HAL_GPIO_WritePin(pin.port, pin.pin, state);
}

bool HardwareManager::begin()
{
    if (ready_)
        return true;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    // Disable all excitation before starting measurements.
    for (Pin pin : BoardPins::rc_ranges) {
        configure_pin(pin, GPIO_MODE_ANALOG);
        write(pin, GPIO_PIN_RESET);
    }
    configure_pin(BoardPins::inductance_pulse, GPIO_MODE_ANALOG);
    configure_pin(BoardPins::inductance_capture, GPIO_MODE_ANALOG);
    configure_pin(BoardPins::resistance_sense, GPIO_MODE_ANALOG);
    configure_pin(BoardPins::capacitance_sense, GPIO_MODE_ANALOG);
    // PA5/PA6 can be connected to PB7/PA15 by the board's I2C bridges.
    configure_pin({GPIOA, GPIO_PIN_5}, GPIO_MODE_INPUT);
    configure_pin({GPIOA, GPIO_PIN_6}, GPIO_MODE_INPUT);
    if (HAL_ADCEx_Calibration_Start(&adc_, ADC_SINGLE_ENDED) != HAL_OK)
        return false;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // Do not reset a shared running counter.
    ready_ = true;
    return true;
}

bool HardwareManager::try_acquire(const void* owner)
{
    if (!ready_ || owner == nullptr)
        return false;
    const void* expected = nullptr;
    return owner_.compare_exchange_strong(expected, owner);
}

void HardwareManager::release(const void* owner)
{
    const void* expected = owner;
    owner_.compare_exchange_strong(expected, nullptr);
}

bool HardwareManager::configure_adc(Pin sense, uint32_t channel, uint32_t sampling_time)
{
    configure_pin(sense, GPIO_MODE_ANALOG);
    ADC_ChannelConfTypeDef config = {};
    config.Channel = channel;
    config.Rank = ADC_REGULAR_RANK_1;
    config.SamplingTime = sampling_time;
    config.SingleDiff = ADC_SINGLE_ENDED;
    config.OffsetNumber = ADC_OFFSET_NONE;
    return HAL_ADC_ConfigChannel(&adc_, &config) == HAL_OK;
}
