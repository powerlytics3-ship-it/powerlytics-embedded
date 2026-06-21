#include "modbus_rtu_master.h"
#include "modbus_crc.h"
#include <string.h>

/* Optional tiny settle after switching to RX (helps some transceivers) */
static void rs485_settle_delay(void)
{
  for (volatile int i = 0; i < 2000; i++) { __NOP(); }
}

void mbm_init(mbm_t *m,
              UART_HandleTypeDef *huart,
              rs485_t *rs,
              uint8_t *rx_buf,
              uint16_t rx_buf_size,
              osSemaphoreId_t rx_sem)
{
  m->huart = huart;
  m->rs485 = rs;
  m->rx_buf = rx_buf;
  m->rx_buf_size = rx_buf_size;
  m->rx_len = 0;
  m->rx_sem = rx_sem;
}

HAL_StatusTypeDef mbm_arm_rx_to_idle(mbm_t *m)
{
  m->rx_len = 0;

  HAL_UART_AbortReceive(m->huart);
  __HAL_UART_CLEAR_IDLEFLAG(m->huart);

  HAL_StatusTypeDef st = HAL_UARTEx_ReceiveToIdle_DMA(m->huart, m->rx_buf, m->rx_buf_size);
  if (m->huart->hdmarx) __HAL_DMA_DISABLE_IT(m->huart->hdmarx, DMA_IT_HT);
  return st;
}

void mbm_on_rx_event(mbm_t *m, UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart != m->huart) return;
  if (size == 0) return;            // ignore spurious idle
  m->rx_len = size;
  osSemaphoreRelease(m->rx_sem);
}

static HAL_StatusTypeDef mbm_tx_frame(mbm_t *m, const uint8_t *buf, uint16_t len)
{
  rs485_tx(m->rs485);

  HAL_StatusTypeDef st = HAL_UART_Transmit(m->huart, (uint8_t*)buf, len, 200);
  if (st != HAL_OK) {
    rs485_rx(m->rs485);
    return st;
  }

  while (__HAL_UART_GET_FLAG(m->huart, UART_FLAG_TC) == RESET) {}
  rs485_rx(m->rs485);
  rs485_settle_delay();
  return HAL_OK;
}

HAL_StatusTypeDef mbm_read_holding_03(mbm_t *m,
                                     uint8_t slave_id,
                                     uint16_t start_reg,
                                     uint16_t qty,
                                     uint32_t timeout_ms,
                                     uint16_t *out_regs)
{
  if (qty < 1 || qty > 125) return HAL_ERROR;

  uint8_t req[8];
  req[0] = slave_id;
  req[1] = 0x03;
  req[2] = (uint8_t)(start_reg >> 8);
  req[3] = (uint8_t)(start_reg & 0xFF);
  req[4] = (uint8_t)(qty >> 8);
  req[5] = (uint8_t)(qty & 0xFF);

  uint16_t crc = mb_crc16(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  /* Arm RX before TX */
  if (mbm_arm_rx_to_idle(m) != HAL_OK) return HAL_ERROR;

  if (mbm_tx_frame(m, req, sizeof(req)) != HAL_OK) return HAL_ERROR;

  if (osSemaphoreAcquire(m->rx_sem, timeout_ms) != osOK) return HAL_TIMEOUT;

  uint16_t n = m->rx_len;
  if (n < 5) return HAL_ERROR;

  if (m->rx_buf[0] != slave_id) return HAL_ERROR;

  uint8_t fc = m->rx_buf[1];
  if (fc & 0x80) return HAL_ERROR;
  if (fc != 0x03) return HAL_ERROR;

  uint8_t bc = m->rx_buf[2];
  uint16_t frame_len = (uint16_t)(3 + bc + 2);
  if (n < frame_len) return HAL_ERROR;

  uint16_t rx_crc = (uint16_t)m->rx_buf[frame_len-2] | ((uint16_t)m->rx_buf[frame_len-1] << 8);
  uint16_t calc   = mb_crc16(m->rx_buf, frame_len-2);
  if (rx_crc != calc) return HAL_ERROR;

  if (bc != qty * 2) return HAL_ERROR;

  for (uint16_t i = 0; i < qty; i++) {
    uint16_t hi = m->rx_buf[3 + (i*2)];
    uint16_t lo = m->rx_buf[4 + (i*2)];
    out_regs[i] = (uint16_t)((hi << 8) | lo);
  }

  return HAL_OK;
}

HAL_StatusTypeDef mbm_read_input_04(mbm_t *m,
                                   uint8_t slave_id,
                                   uint16_t start_reg,
                                   uint16_t qty,
                                   uint32_t timeout_ms,
                                   uint16_t *out_regs)
{
  if (qty < 1 || qty > 125) return HAL_ERROR;

  uint8_t req[8];
  req[0] = slave_id;
  req[1] = 0x04;
  req[2] = (uint8_t)(start_reg >> 8);
  req[3] = (uint8_t)(start_reg & 0xFF);
  req[4] = (uint8_t)(qty >> 8);
  req[5] = (uint8_t)(qty & 0xFF);

  uint16_t crc = mb_crc16(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  /* Arm RX before TX */
  if (mbm_arm_rx_to_idle(m) != HAL_OK) return HAL_ERROR;

  if (mbm_tx_frame(m, req, sizeof(req)) != HAL_OK) return HAL_ERROR;

  if (osSemaphoreAcquire(m->rx_sem, timeout_ms) != osOK) return HAL_TIMEOUT;

  uint16_t n = m->rx_len;
  if (n < 5) return HAL_ERROR;

  if (m->rx_buf[0] != slave_id) return HAL_ERROR;

  uint8_t fc = m->rx_buf[1];
  if (fc & 0x80) return HAL_ERROR;
  if (fc != 0x04) return HAL_ERROR;

  uint8_t bc = m->rx_buf[2];
  uint16_t frame_len = (uint16_t)(3 + bc + 2);
  if (n < frame_len) return HAL_ERROR;

  uint16_t rx_crc = (uint16_t)m->rx_buf[frame_len-2] | ((uint16_t)m->rx_buf[frame_len-1] << 8);
  uint16_t calc   = mb_crc16(m->rx_buf, frame_len-2);
  if (rx_crc != calc) return HAL_ERROR;

  if (bc != qty * 2) return HAL_ERROR;

  for (uint16_t i = 0; i < qty; i++) {
    uint16_t hi = m->rx_buf[3 + (i*2)];
    uint16_t lo = m->rx_buf[4 + (i*2)];
    out_regs[i] = (uint16_t)((hi << 8) | lo);
  }

  return HAL_OK;
}
