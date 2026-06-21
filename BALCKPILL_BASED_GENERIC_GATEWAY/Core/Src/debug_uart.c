#include "debug_uart.h"
#include <string.h>
#include <stdio.h>     // <-- needed for vsnprintf
#include <stdarg.h>

static UART_HandleTypeDef *dbg_huart = NULL;

void debug_uart_init(UART_HandleTypeDef *huart) {
    dbg_huart = huart;
}

void print(const char *fmt, ...) {
    if (!dbg_huart) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    HAL_UART_Transmit(dbg_huart, (uint8_t*)buf, strlen(buf), 1000);
}
