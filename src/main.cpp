#include "app.h"
#include "capacitance.h"

extern "C" {
extern ADC_HandleTypeDef hadc1;
}

namespace
{
CapacitanceMeter c(hadc1);
}

void setup(void)
{
    if (c.activate() != CapacitanceMeter::Status::Ok)
        Error_Handler();
}

void loop(void)
{
    float capacitance = c.read_measurement();
    (void)capacitance; // Use capacitance in the component meter when ready.
}
