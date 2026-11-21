#include "buttons.h"

void Buttons::begin()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    for (unsigned i = 0; i < 2; ++i) {
        GPIO_InitTypeDef config = {};
        config.Pin = pins_[i].pin;
        config.Mode = GPIO_MODE_INPUT;
        config.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(pins_[i].port, &config);
        const bool pressed = HAL_GPIO_ReadPin(pins_[i].port, pins_[i].pin) == GPIO_PIN_RESET;
        // A switch held during startup must be released before generating a press.
        states_[i] = {pressed, pressed, HAL_GetTick()};
    }
    ready_ = true;
}

bool Buttons::update(unsigned index, uint32_t now)
{
    auto& state = states_[index];
    const bool pressed = HAL_GPIO_ReadPin(pins_[index].port, pins_[index].pin) == GPIO_PIN_RESET;
    if (pressed != state.candidate) {
        state.candidate = pressed;
        state.changed_ms = now;
    }
    if (state.stable != state.candidate && now - state.changed_ms >= debounce_ms) {
        state.stable = state.candidate;
        return state.stable;
    }
    return false;
}

Buttons::Events Buttons::poll()
{
    if (!ready_)
        return {};
    const uint32_t now = HAL_GetTick();
    return {update(0, now), update(1, now)};
}
