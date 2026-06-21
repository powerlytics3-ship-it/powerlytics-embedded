#pragma once
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "rs485.h"
#include <stdint.h>

typedef struct {
  UART_HandleTypeDef *huart;
  rs485_t            *rs485;

  uint8_t            *rx_buf;
  uint16_t            rx_buf_size;

  volatile uint16_t   rx_len;
  osSemaphoreId_t     rx_sem;
} mbm_t;

/**
 * @brief Initialize Modbus master context (does not create RTOS objects).
 * @note  Call mbm_arm_rx_to_idle() once before first transaction.
 */
void mbm_init(mbm_t *m,
              UART_HandleTypeDef *huart,
              rs485_t *rs,
              uint8_t *rx_buf,
              uint16_t rx_buf_size,
              osSemaphoreId_t rx_sem);

/**
 * @brief Arm UART Receive-to-IDLE DMA for Modbus RTU.
 * @note  Aborts any existing RX to avoid HAL_BUSY and clears IDLE flag.
 */
HAL_StatusTypeDef mbm_arm_rx_to_idle(mbm_t *m);

/**
 * @brief Must be called from HAL_UARTEx_RxEventCallback to capture frame length and release semaphore.
 */
void mbm_on_rx_event(mbm_t *m, UART_HandleTypeDef *huart, uint16_t size);

/**
 * @brief Send Modbus function 0x03 (Read Holding Registers) and parse response.
 * @param slave_id Modbus slave id
 * @param start_reg Start register address
 * @param qty Number of registers (1..125)
 * @param timeout_ms Wait timeout for response
 * @param out_regs Output array length >= qty
 * @return HAL_OK on success, HAL_TIMEOUT on timeout, HAL_ERROR on parse/CRC issues.
 */
HAL_StatusTypeDef mbm_read_holding_03(mbm_t *m,
                                     uint8_t slave_id,
                                     uint16_t start_reg,
                                     uint16_t qty,
                                     uint32_t timeout_ms,
                                     uint16_t *out_regs);

HAL_StatusTypeDef mbm_read_input_04(mbm_t *m,
                                   uint8_t slave_id,
                                   uint16_t start_reg,
                                   uint16_t qty,
                                   uint32_t timeout_ms,
                                   uint16_t *out_regs);
