#ifndef ECHO_APP_H
#define ECHO_APP_H

// HAL, board functions, pin names and Error_Handler from CubeMX.
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

void setup(void);
void loop(void);

#ifdef HAL_I2C_MODULE_ENABLED
// Defined by CubeMX once I2C1 is enabled in ECHO/ECHO.ioc.
extern I2C_HandleTypeDef hi2c1;
#endif

#ifdef __cplusplus
}
#endif

#endif
