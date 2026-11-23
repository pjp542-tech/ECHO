#include "app.h"
#include "diode.h"
#include <cstdio>

extern "C" {
extern ADC_HandleTypeDef hadc1;
}

namespace
{
DiodeMeter diode_meter(hadc1);
}

void setup(void)
{
if (diode_meter.activate() != DiodeMeter::Status::Ok)
Error_Handler();
}

void loop(void)
{
DiodeMeter::Measurement m = diode_meter.read_measurement();
int vf_mv = static_cast<int>(m.forward_voltage_v * 1000.0f + 0.5f);
int if_ua = static_cast<int>(m.forward_current_ma * 1000.0f + 0.5f);

switch (m.state)
{
case DiodeMeter::ResultState::GoodForward:
std::printf("Diode OK (A->B) | Vf: %d mV | If: %d uA\r\n", vf_mv, if_ua);
break;
case DiodeMeter::ResultState::GoodReverse:
std::printf("Diode OK (B->A) | Vf: %d mV | If: %d uA\r\n", vf_mv, if_ua);
break;
case DiodeMeter::ResultState::OpenCircuit:
std::printf("Open circuit / No diode\r\n");
break;
case DiodeMeter::ResultState::ShortCircuit:
std::printf("Shorted diode / Wire\r\n");
break;
case DiodeMeter::ResultState::FaultyLeaky:
std::printf("Faulty / Leaky / Resistor (Vf: %d mV)\r\n", vf_mv);
break;
default:
std::printf("Diode read error\r\n");
break;
}

HAL_Delay(500);
}
