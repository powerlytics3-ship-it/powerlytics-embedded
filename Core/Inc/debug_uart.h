#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#include "stm32f4xx_hal.h"
#include <stdarg.h>

void debug_uart_init(UART_HandleTypeDef *huart);
void print(const char *fmt, ...);   // <-- variadic now

#endif
