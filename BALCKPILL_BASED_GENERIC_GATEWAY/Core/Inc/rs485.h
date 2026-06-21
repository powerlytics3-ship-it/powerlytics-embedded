#pragma once
#include "stm32f4xx_hal.h"
#include <stdint.h>

/**
 * @brief Simple RS-485 direction control using DE pin (and optionally /RE tied to DE).
 */
typedef struct {
  GPIO_TypeDef *de_port;
  uint16_t      de_pin;
} rs485_t;

/**
 * @brief Initialize DE GPIO as push-pull output and set default RX mode.
 * @note  GPIO clock must already be enabled for de_port (or enable before calling).
 */
void rs485_init(rs485_t *h, GPIO_TypeDef *de_port, uint16_t de_pin);

/** @brief Set transceiver to transmit mode (DE=1). */
static inline void rs485_tx(const rs485_t *h) { HAL_GPIO_WritePin(h->de_port, h->de_pin, GPIO_PIN_SET); }

/** @brief Set transceiver to receive mode (DE=0). */
static inline void rs485_rx(const rs485_t *h) { HAL_GPIO_WritePin(h->de_port, h->de_pin, GPIO_PIN_RESET); }
