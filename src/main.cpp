#include "app.h"
#include "inductance.h"

extern "C" {
extern TIM_HandleTypeDef htim2;
}

namespace
{
InductanceMeter l(htim2);
}

void setup(void)
{
    if (l.activate() != InductanceMeter::Status::Ok)
        Error_Handler();
}

void loop(void)
{
    float inductance = l.read_measurement();
    (void)inductance; // Use inductance in the component meter when ready.
    HAL_Delay(500); // Preserve the interval between LC excitation pulses.
}
