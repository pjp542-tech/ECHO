#include "app.h"
#include "resistance.h"

extern "C" {
extern ADC_HandleTypeDef hadc1;
}

namespace
{
ResistanceMeter r(hadc1);
}

void setup(void)
{
    if (r.activate() != ResistanceMeter::Status::Ok)
        Error_Handler();
}

void loop(void)
{
    float resistance = r.read_measurement();
    (void)resistance; // Use resistance in the component meter when ready.
}
