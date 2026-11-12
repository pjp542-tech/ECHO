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
        if (C < 1e-9f){
            std::printf("C = %lu pF\r\n", (unsigned long)(C * 1e12f + 0.5f));
        }
        else if (C < 1e-6f){
            std::printf("C = %lu nF\r\n", (unsigned long)(C * 1e9f + 0.5f));
        }
        else{
            std::printf("C = %lu uF\r\n", (unsigned long)(C * 1e6f + 0.5f));
        }
    }
    else if (C == cap_too_large) {

        std::printf("capacitance out of range: too large \r\n");
    }
    else if (C == cap_too_small) {

        std::printf("capacitance out of range: too small \r\n");
    }
    else{

        std::printf("unexpected error: %d \r\n", (int)(C));
    }
    HAL_Delay(1000); 
}
