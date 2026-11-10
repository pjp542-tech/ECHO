#include "app.h"
#include <cstdio>
#include <cstdint>

// Timer created by CubeMX.
extern "C" {
extern TIM_HandleTypeDef htim2;
}

// CIRCUIT VALUES
const float capacitor = 0.00000001f; // 10 nF in farads
const float pi = 3.1415927f;

// At 10 MHz, the timer counts 10 times per microsecond.
const float ticksPerMicrosecond = 10.0f;

// MEASURED VALUES
float frequency = 0.0f;
float inductance = 0.0f;

// PIN FUNCTIONS

// Change PA4 between an output and a disconnected input.
void setPulseOutput(bool output){
    GPIO_InitTypeDef pin = {};

    pin.Pin = GPIO_PIN_4;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;

    if (output == true){
        pin.Mode = GPIO_MODE_OUTPUT_PP;
    }
    else{
        // Analogue mode leaves the pin high impedance.
        pin.Mode = GPIO_MODE_ANALOG;
    }

    HAL_GPIO_Init(GPIOA, &pin);
}

// Send a short pulse into the LC circuit.
void sendPulse(){
    // Prepare HIGH before enabling the output.
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    setPulseOutput(true);

    uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);
    uint32_t elapsed = 0;

    // 20 ticks at 10 MHz = 2 microseconds.
    while (elapsed < 20){
        elapsed = __HAL_TIM_GET_COUNTER(&htim2) - start;
    }

    // Disconnect the output so the circuit can oscillate.
    setPulseOutput(false);
}


// MEASUREMENT FUNCTIONS

// Wait for the comparator output to change from LOW to HIGH.
bool waitForEdge(){
    uint32_t start = HAL_GetTick();

    while (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_CC2) == RESET){
        // Stop waiting after 5 milliseconds.
        if (HAL_GetTick() - start >= 5){
            return false;
        }
    }
    return true;
}


// Return one oscillation period in microseconds.
// Return zero if the measurement fails.
float measurePeriod()
{
__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC2 | TIM_FLAG_CC2OF);

if (!waitForEdge())
{
std::printf("Debug: no first edge\r\n");
return 0.0f;
}

uint32_t firstEdge =
HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_2);

if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_CC2OF) != RESET)
{
std::printf("Debug: capture overrun at first edge\r\n");
return 0.0f;
}

if (!waitForEdge())
{
std::printf("Debug: first edge seen, no second edge\r\n");
return 0.0f;
}

uint32_t secondEdge =
HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_2);

if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_CC2OF) != RESET)
{
std::printf("Debug: capture overrun at second edge\r\n");
return 0.0f;
}

uint32_t elapsedTicks = secondEdge - firstEdge;
std::printf("Debug: %lu ticks\r\n", (unsigned long)elapsedTicks);

return elapsedTicks / ticksPerMicrosecond;
}

// STARTUP
void setup(void){
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Leave the excitation pin disconnected initially.
    setPulseOutput(false);

    // Start the timer and its input capture.
    if (HAL_TIM_IC_Start(&htim2, TIM_CHANNEL_2) != HAL_OK){
        Error_Handler();
    }

    HAL_Delay(20);
}


// MAIN PROGRAM
void loop(void){
    frequency = 0.0f;
    inductance = 0.0f;

    sendPulse();

    float period = measurePeriod() * 1.1f; // Add 10%.

    // Initial acceptance window: 5 to 100 microseconds.
    // This is a basic check, not proof of a valid measurement.
    if (period >= 5.0f && period <= 100.0f){
        // Convert period in microseconds to frequency in Hz.
        frequency = 1000000.0f / period;

        // Calculate inductance
        inductance = 1.0f / (4.0f * pi * pi * frequency * frequency * capacitor);
        // Convert to whole numbers for simple serial printing.
        int frequency_Hz = (int)(frequency + 0.5f);
        int inductance_mH = (int)(inductance * 1000000.0f + 0.5f);
        std::printf(
            "Frequency: %d Hz | Inductance: %d uH\r\n",
            frequency_Hz,
            inductance_mH
        );
    }
    else{
        std::printf("No valid inductance reading\r\n");
    }
    HAL_Delay(500);
}