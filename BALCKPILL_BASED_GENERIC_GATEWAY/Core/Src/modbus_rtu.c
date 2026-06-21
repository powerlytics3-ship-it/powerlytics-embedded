// modbus_rtu.c  — Blocking (non-DMA) Modbus RTU master for USART3
#include "modbus_rtu.h"
#include "debug_uart.h"
#include <string.h>
#include <stdint.h>

extern UART_HandleTypeDef huart3;   // provided by your project (MX_USART3_UART_Init)

#ifndef MODBUS_RX_BUFSIZE
#define MODBUS_RX_BUFSIZE 256
#endif

#ifndef MODBUS_TX_GUARD_MS
#define MODBUS_TX_GUARD_MS 1     // pre/post TX guard around DE toggles
#endif

#ifndef MODBUS_RX_TIMEOUT_MS
#define MODBUS_RX_TIMEOUT_MS 1 // overall response window (adjust for lower baud)
#endif

#ifndef MB_DEFAULT_BAUD
#define MB_DEFAULT_BAUD 115200   // keep this in sync with huart3.Init.BaudRate
#endif

#ifndef MB_BITS_PER_CHAR
#define MB_BITS_PER_CHAR 11      // 8E1 ≈ 11 bits; 8N1 ≈ 10
#endif

static uint8_t rxBuf[MODBUS_RX_BUFSIZE];

/* -------- Direction control (your macros must map these correctly) -------- */
static inline void dir_tx(void) {
    HAL_GPIO_WritePin(MODBUS_DE_GPIO_Port, MODBUS_DE_Pin, GPIO_PIN_SET);   // HIGH = TX
}
static inline void dir_rx(void) {
    HAL_GPIO_WritePin(MODBUS_DE_GPIO_Port, MODBUS_DE_Pin, GPIO_PIN_RESET); // LOW  = RX
}

/* -------- Helpers -------- */
static inline uint32_t mb_char_time_us(uint32_t baud) {
    float us = (1000000.0f * (float)MB_BITS_PER_CHAR) / (float)baud;
    return (uint32_t)(us + 0.5f);
}
static inline void mb_delay_char_times(uint32_t chars, uint32_t baud) {
    uint32_t us = mb_char_time_us(baud) * chars;
    HAL_Delay((us + 999) / 1000);
}

/* -------- CRC16 (Modbus) -------- */
static uint16_t mb_crc16(const uint8_t *buf, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}

/* -------- Init -------- */
void ModbusRTU_Init(void)
{
    // Ensure DE/RE pin exists and defaults to RX
    GPIO_InitTypeDef gi = {0};
    gi.Pin   = MODBUS_DE_Pin;
    gi.Mode  = GPIO_MODE_OUTPUT_PP;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(MODBUS_DE_GPIO_Port, &gi);
    dir_rx();

    // Flush UART error flags
    __HAL_UART_FLUSH_DRREGISTER(&huart3);
    __HAL_UART_CLEAR_OREFLAG(&huart3);
    __HAL_UART_CLEAR_NEFLAG(&huart3);
    __HAL_UART_CLEAR_FEFLAG(&huart3);
    __HAL_UART_CLEAR_PEFLAG(&huart3);

    print("ModbusRTU: init OK (UART3, DE on %c%d)\r\n",
          (MODBUS_DE_GPIO_Port==GPIOA?'A':(MODBUS_DE_GPIO_Port==GPIOB?'B':(MODBUS_DE_GPIO_Port==GPIOC?'C':'X'))),
          (int)__builtin_ctz(MODBUS_DE_Pin));
}

/* -------- Blocking TX/RX exchange (no DMA) --------
   - Arms RX by switching DE->RX
   - Reads bytes until either 'expect_len' is met or inter-char silence occurs
   - Times out at MODBUS_RX_TIMEOUT_MS
*/
static int mb_exchange(const uint8_t *tx, size_t tx_len,
                       uint8_t *rx, size_t rx_max,
                       uint32_t expect_len)
{
    /* --- TX side --- */
    dir_tx();
    HAL_Delay(MODBUS_TX_GUARD_MS);   // pre-guard

    // Clear lingering UART errors before TX
    __HAL_UART_CLEAR_OREFLAG(&huart3);
    __HAL_UART_CLEAR_NEFLAG(&huart3);
    __HAL_UART_CLEAR_FEFLAG(&huart3);
    __HAL_UART_CLEAR_PEFLAG(&huart3);
    __HAL_UART_FLUSH_DRREGISTER(&huart3);

    if (HAL_UART_Transmit(&huart3, (uint8_t*)tx, tx_len, 100) != HAL_OK) {
        dir_rx();
        return -1; // TX error
    }
    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET) {} // wait for full shift
    HAL_Delay(MODBUS_TX_GUARD_MS);   // post-guard

    /* --- Switch to RX --- */
    dir_rx();

    // Small turnaround (~2 char times) so we don't miss 1st byte
    mb_delay_char_times(2, MB_DEFAULT_BAUD);

    // Clear flags again at RX window start
    __HAL_UART_CLEAR_OREFLAG(&huart3);
    __HAL_UART_CLEAR_NEFLAG(&huart3);
    __HAL_UART_CLEAR_FEFLAG(&huart3);
    __HAL_UART_CLEAR_PEFLAG(&huart3);
    __HAL_UART_FLUSH_DRREGISTER(&huart3);

    /* --- RX loop --- */
    size_t   got = 0;
    uint32_t t0  = HAL_GetTick();
    // Per-byte wait (jitter tolerant):
    const uint32_t PER_BYTE_TO_MS = 5;

    // Inter-character silence threshold (~3 char times, in ms)
    uint32_t ichar_timeout_ms = ((MB_BITS_PER_CHAR * 3) * 1000U) / MB_DEFAULT_BAUD;
    if (ichar_timeout_ms < 2) ichar_timeout_ms = 2;

    uint32_t last_byte_tick = t0;

    while ((HAL_GetTick() - t0) < MODBUS_RX_TIMEOUT_MS) {
        uint8_t b;
        if (HAL_UART_Receive(&huart3, &b, 1, PER_BYTE_TO_MS) == HAL_OK) {
            if (got < rx_max) rx[got] = b;
            got++;
            last_byte_tick = HAL_GetTick();

            if (expect_len > 0 && got >= expect_len) break; // whole frame known
        } else {
            // No byte this slice. If we already have some data and saw a silence gap, stop.
            if (got > 0 && (HAL_GetTick() - last_byte_tick) > ichar_timeout_ms) {
                break; // line went silent (end of RTU frame)
            }
            // Otherwise keep waiting until overall timeout
        }
    }

    if (expect_len > 0 && got < expect_len) {
        print("ModbusRTU: timeout, got %u of %lu\r\n", (unsigned)got, (unsigned long)expect_len);
        return -2;
    }
    if (got < 5) { // too small to be a valid frame
        print("ModbusRTU: short resp %u bytes\r\n", (unsigned)got);
        return -3;
    }
    return (int)(got > rx_max ? rx_max : got);
}

/* ================= High-level APIs ================= */

int ModbusRTU_ReadHoldingRegs(uint8_t slave, uint16_t start, uint16_t qty,
                              uint16_t *out, size_t out_len)
{
    if (qty == 0 || out_len < qty) return -10;

    uint8_t tx[8];
    tx[0] = slave;
    tx[1] = 0x03;
    tx[2] = (uint8_t)(start >> 8);
    tx[3] = (uint8_t)(start & 0xFF);
    tx[4] = (uint8_t)(qty >> 8);
    tx[5] = (uint8_t)(qty & 0xFF);
    uint16_t crc = mb_crc16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);
    tx[7] = (uint8_t)(crc >> 8);

    // Expect: addr + func + bc + (qty*2) + CRC(2)
    uint32_t expect = 1 + 1 + 1 + (uint32_t)qty * 2 + 2;

    int rlen = mb_exchange(tx, sizeof(tx), rxBuf, sizeof(rxBuf), expect);

    if (rlen < 0) return rlen;

    if (rxBuf[0] != slave)                   { print("ModbusRTU: bad slave 0x%02X\r\n", rxBuf[0]); return -11; }
    if (rxBuf[1] == (0x80 | 0x03))           { print("ModbusRTU: exception code %u\r\n", rxBuf[2]); return -12; }
    if (rxBuf[1] != 0x03)                    { print("ModbusRTU: unexpected func 0x%02X\r\n", rxBuf[1]); return -13; }
    if (rxBuf[2] != qty*2)                   { print("ModbusRTU: bytecount %u != %u\r\n", rxBuf[2], (unsigned)(qty*2)); return -14; }

    uint16_t rx_crc = ((uint16_t)rxBuf[expect-1] << 8) | rxBuf[expect-2];
    uint16_t crc2   = mb_crc16(rxBuf, expect-2);
    if (rx_crc != crc2)                      { print("ModbusRTU: CRC mismatch (%04X vs %04X)\r\n", rx_crc, crc2); return -15; }

    for (uint16_t i = 0; i < qty; ++i) {
        out[i] = ((uint16_t)rxBuf[3 + i*2] << 8) | rxBuf[3 + i*2 + 1];
    }
    return (int)qty;
}

int ModbusRTU_ReadInputRegs(uint8_t slave, uint16_t start, uint16_t qty,
                            uint16_t *out, size_t out_len)
{
    if (qty == 0 || out_len < qty) return -20;

    uint8_t tx[8];
    tx[0] = slave;
    tx[1] = 0x04;
    tx[2] = (uint8_t)(start >> 8);
    tx[3] = (uint8_t)(start & 0xFF);
    tx[4] = (uint8_t)(qty >> 8);
    tx[5] = (uint8_t)(qty & 0xFF);
    uint16_t crc = mb_crc16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);
    tx[7] = (uint8_t)(crc >> 8);

    uint32_t expect = 1 + 1 + 1 + (uint32_t)qty * 2 + 2;

    int rlen = mb_exchange(tx, sizeof(tx), rxBuf, sizeof(rxBuf), expect);
    if (rlen < 0) return rlen;

    if (rxBuf[0] != slave)                   { print("ModbusRTU: bad slave 0x%02X (IR)\r\n", rxBuf[0]); return -21; }
    if (rxBuf[1] == (0x80 | 0x04))           { print("ModbusRTU: exception (IR) code %u\r\n", rxBuf[2]); return -22; }
    if (rxBuf[1] != 0x04)                    { print("ModbusRTU: unexpected func 0x%02X (IR)\r\n", rxBuf[1]); return -23; }
    if (rxBuf[2] != qty*2)                   { print("ModbusRTU: bytecount %u != %u (IR)\r\n", rxBuf[2], (unsigned)(qty*2)); return -24; }

    uint16_t rx_crc = ((uint16_t)rxBuf[expect-1] << 8) | rxBuf[expect-2];
    uint16_t crc2   = mb_crc16(rxBuf, expect-2);
    if (rx_crc != crc2)                      { print("ModbusRTU: CRC mismatch (IR) %04X vs %04X\r\n", rx_crc, crc2); return -25; }

    for (uint16_t i = 0; i < qty; ++i) {
        out[i] = ((uint16_t)rxBuf[3 + i*2] << 8) | rxBuf[3 + i*2 + 1];
    }
    return (int)qty;
}

int ModbusRTU_ReadCoils(uint8_t slave, uint16_t start, uint16_t qty,
                        uint8_t *out_bits, size_t out_bits_len)
{
    if (qty == 0 || out_bits_len < qty) return -30;

    uint8_t tx[8];
    tx[0] = slave;
    tx[1] = 0x01;
    tx[2] = (uint8_t)(start >> 8);
    tx[3] = (uint8_t)(start & 0xFF);
    tx[4] = (uint8_t)(qty >> 8);
    tx[5] = (uint8_t)(qty & 0xFF);
    uint16_t crc = mb_crc16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);
    tx[7] = (uint8_t)(crc >> 8);

    uint32_t data_bytes = (qty + 7u) / 8u;
    uint32_t expect = 1 + 1 + 1 + data_bytes + 2;

    int rlen = mb_exchange(tx, sizeof(tx), rxBuf, sizeof(rxBuf), expect);
    if (rlen < 0) return rlen;

    if (rxBuf[0] != slave)                   { print("ModbusRTU: bad slave 0x%02X (CO)\r\n", rxBuf[0]); return -31; }
    if (rxBuf[1] == (0x80 | 0x01))           { print("ModbusRTU: exception (CO) code %u\r\n", rxBuf[2]); return -32; }
    if (rxBuf[1] != 0x01)                    { print("ModbusRTU: unexpected func 0x%02X (CO)\r\n", rxBuf[1]); return -33; }
    if (rxBuf[2] != data_bytes)              { print("ModbusRTU: bytecount %u != %lu (CO)\r\n", rxBuf[2], (unsigned long)data_bytes); return -34; }

    uint16_t rx_crc = ((uint16_t)rxBuf[expect-1] << 8) | rxBuf[expect-2];
    uint16_t crc2   = mb_crc16(rxBuf, expect-2);
    if (rx_crc != crc2)                      { print("ModbusRTU: CRC mismatch (CO) %04X vs %04X\r\n", rx_crc, crc2); return -35; }

    for (uint16_t i = 0; i < qty; ++i) {
        uint8_t byte = rxBuf[3 + (i >> 3)];
        out_bits[i]  = (byte >> (i & 7)) & 0x01; // LSB first
    }
    return (int)qty;
}

int ModbusRTU_ReadDiscreteInputs(uint8_t slave, uint16_t start, uint16_t qty,
                                 uint8_t *out_bits, size_t out_bits_len)
{
    if (qty == 0 || out_bits_len < qty) return -40;

    uint8_t tx[8];
    tx[0] = slave;
    tx[1] = 0x02;
    tx[2] = (uint8_t)(start >> 8);
    tx[3] = (uint8_t)(start & 0xFF);
    tx[4] = (uint8_t)(qty >> 8);
    tx[5] = (uint8_t)(qty & 0xFF);
    uint16_t crc = mb_crc16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);
    tx[7] = (uint8_t)(crc >> 8);

    uint32_t data_bytes = (qty + 7u) / 8u;
    uint32_t expect = 1 + 1 + 1 + data_bytes + 2;

    int rlen = mb_exchange(tx, sizeof(tx), rxBuf, sizeof(rxBuf), expect);
    if (rlen < 0) return rlen;

    if (rxBuf[0] != slave)                   { print("ModbusRTU: bad slave 0x%02X (DI)\r\n", rxBuf[0]); return -41; }
    if (rxBuf[1] == (0x80 | 0x02))           { print("ModbusRTU: exception (DI) code %u\r\n", rxBuf[2]); return -42; }
    if (rxBuf[1] != 0x02)                    { print("ModbusRTU: unexpected func 0x%02X (DI)\r\n", rxBuf[1]); return -43; }
    if (rxBuf[2] != data_bytes)              { print("ModbusRTU: bytecount %u != %lu (DI)\r\n", rxBuf[2], (unsigned long)data_bytes); return -44; }

    uint16_t rx_crc = ((uint16_t)rxBuf[expect-1] << 8) | rxBuf[expect-2];
    uint16_t crc2   = mb_crc16(rxBuf, expect-2);
    if (rx_crc != crc2)                      { print("ModbusRTU: CRC mismatch (DI) %04X vs %04X\r\n", rx_crc, crc2); return -45; }

    for (uint16_t i = 0; i < qty; ++i) {
        uint8_t byte = rxBuf[3 + (i >> 3)];
        out_bits[i]  = (byte >> (i & 7)) & 0x01;
    }
    return (int)qty;
}

/* --------------- Optional debug helper ---------------
static void dump_rx(const uint8_t* b, int n) {
    print("RX(%d): ", n);
    for (int i = 0; i < n; ++i) print("%02X ", b[i]);
    print("\r\n");
}
------------------------------------------------------ */

void ModbusRTU_PrintRegs(const char *tag, const uint16_t *regs, size_t n)
{
    print("MB %s: ", tag ? tag : "");
    for (size_t i = 0; i < n; ++i) {
        print("%u%s", (unsigned)regs[i], (i + 1 < n) ? "," : "");
    }
    print("\r\n");
}

