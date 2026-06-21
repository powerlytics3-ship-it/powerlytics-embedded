#ifndef W25Q16_H
#define W25Q16_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ===================== CONFIG ===================== */
#define W25Q_PAGE_SIZE        256U
#define W25Q_SECTOR_SIZE      4096U
#define W25Q_TOTAL_SIZE       (2 * 1024 * 1024)   // 2MB

/* ===================== HANDLE ===================== */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
} W25Q_Handle;

/* ===================== API ===================== */
bool     w25q_init(W25Q_Handle *dev);
uint32_t w25q_read_jedec_id(W25Q_Handle *dev);

bool w25q_read(W25Q_Handle *dev, uint32_t addr, uint8_t *buf, uint32_t len);
bool w25q_write(W25Q_Handle *dev, uint32_t addr, const uint8_t *buf, uint32_t len);

bool w25q_erase_sector_4k(W25Q_Handle *dev, uint32_t addr);

bool w25q_is_busy(W25Q_Handle *dev);
bool w25q_wait_ready(W25Q_Handle *dev, uint32_t timeout_ms);

#endif /* W25Q16_H */
