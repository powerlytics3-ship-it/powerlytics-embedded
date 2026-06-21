#pragma once
#include <stdint.h>

/**
 * @brief Compute Modbus RTU CRC16.
 * @param buf Data buffer
 * @param len Number of bytes
 * @return CRC16 value (LSB-first when placed on wire).
 */
uint16_t mb_crc16(const uint8_t *buf, uint16_t len);
