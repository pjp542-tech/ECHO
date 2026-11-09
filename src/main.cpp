#include "app.h"
#include "capacitance.h"

#include <cstdio>

// Runs once after the board is ready.
void setup(void)
{
    capacitance_init();
    // Put your initialisation code here.
}

// Runs repeatedly.
void loop(void)
{
    float c = capacitance_measure();
    std::printf("C = %lu pF\r\n", (unsigned long)(c * 1e12f + 0.5f));
    HAL_Delay(1000);
    // Put your application code here.
}
//Treya commented