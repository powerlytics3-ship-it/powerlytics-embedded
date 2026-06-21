#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include "stm32f4xx_hal.h"   // adjust to your MCU family if needed
#include <stdint.h>
#include <stddef.h>

// --------- User configuration ----------
// Tie MAX485 RE and DE together to one MCU pin (active HIGH = transmit)
#define MODBUS_DE_GPIO_Port   GPIOA
#define MODBUS_DE_Pin         GPIO_PIN_5   // <- change if you wired a different pin

// UART for Modbus (your huart1)
extern UART_HandleTypeDef huart3;

// Timeouts (ms)
#define MODBUS_TX_GUARD_MS    0
#define MODBUS_RX_TIMEOUT_MS  100

// RX buffer
#define MODBUS_RX_BUFSIZE     256

// --------- API ----------
void ModbusRTU_Init(void);

// Read Holding Registers (0x03)
// returns number of 16-bit registers placed into out[] on success, <0 on error
int ModbusRTU_ReadHoldingRegs(uint8_t slave, uint16_t start, uint16_t qty, uint16_t *out, size_t out_len);

// Optional: helpers to print what you got
void ModbusRTU_PrintRegs(const char *tag, const uint16_t *regs, size_t n);

// Stubs you can implement later
int ModbusRTU_ReadInputRegs(uint8_t slave, uint16_t start, uint16_t qty, uint16_t *out, size_t out_len);
int ModbusRTU_ReadCoils(uint8_t slave, uint16_t start, uint16_t qty, uint8_t *out_bits, size_t out_bits_len);
int ModbusRTU_ReadDiscreteInputs(uint8_t slave, uint16_t start, uint16_t qty, uint8_t *out_bits, size_t out_bits_len);
void MB_OnUartIdle(UART_HandleTypeDef *huart);

#endif
