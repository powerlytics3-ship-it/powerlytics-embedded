#pragma once
#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stddef.h>

#ifndef U2_DMA_RX_SZ
#define U2_DMA_RX_SZ (1024 * 4)
#endif

#ifndef U2_RING_SZ
#define U2_RING_SZ     8192
#endif

typedef struct {
    UART_HandleTypeDef *huart;
    uint8_t  dma_buf[U2_DMA_RX_SZ];

    uint8_t  ring[U2_RING_SZ];
    volatile uint32_t wr;     // write index
    volatile uint32_t rd;     // read index

    osSemaphoreId_t rxSem;    // released on RX idle event
} uart2_rx_t;

void     u2rx_init(uart2_rx_t *u2, UART_HandleTypeDef *huart, osSemaphoreId_t sem);
HAL_StatusTypeDef u2rx_start(uart2_rx_t *u2);
void     u2rx_on_idle_event(uart2_rx_t *u2, uint16_t size);

size_t   u2rx_available(uart2_rx_t *u2);
size_t   u2rx_read(uart2_rx_t *u2, uint8_t *out, size_t max);
void     u2rx_drop_all(uart2_rx_t *u2);

int      u2rx_read_line(uart2_rx_t *u2, char *out, int out_sz, uint32_t timeout_ms);
int      u2rx_wait_for(uart2_rx_t *u2, const char *needle, uint32_t timeout_ms);

/* Helpers */
int      u2rx_find_and_copy_between(uart2_rx_t *u2,
                                   const char *start_tag,
                                   const char *end_tag,
                                   char *out, int out_sz,
                                   uint32_t timeout_ms);
