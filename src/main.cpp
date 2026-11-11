#include "app.h"
#include "capacitance.h"

#include <cstdio>

// Runs once after the board is ready.
void setup(void)
{
    capacitance_init();
}

// Runs repeatedly.
void loop(void)
{
    float C = capacitance_measure();
    if (C > 0){
        std::printf("C = %lu pF\r\n", (unsigned long)(C * 1e12f + 0.5f));
    }
    else if (C == -1) {

        std::printf("capacitance out of range: too large \r\n");
    }
    else if (C == -2) {

        std::printf("capacitance out of range: too small \r\n");
    }
    else{

        std::printf("unexpected error: %d \r\n", (int)(C));
    }
    HAL_Delay(1000); 
}
