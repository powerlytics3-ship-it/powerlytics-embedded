#include "w25q16.h"
#include <string.h>

/* ===================== COMMANDS ===================== */
#define CMD_JEDEC_ID        0x9F
#define CMD_READ_DATA       0x03
#define CMD_WRITE_ENABLE    0x06
#define CMD_PAGE_PROGRAM    0x02
#define CMD_READ_SR1        0x05
#define CMD_SECTOR_ERASE_4K 0x20

#define SR1_BUSY            (1U << 0)

/* ===================== LOW LEVEL ===================== */
static inline void cs_low(W25Q_Handle *d) {
    HAL_GPIO_WritePin(d->cs_port, d->cs_pin, GPIO_PIN_RESET);
}
static inline void cs_high(W25Q_Handle *d) {
    HAL_GPIO_WritePin(d->cs_port, d->cs_pin, GPIO_PIN_SET);
}

static bool spi_tx(W25Q_Handle *d, const uint8_t *buf, uint16_t len) {
    return (HAL_SPI_Transmit(d->hspi, (uint8_t *)buf, len, 1000) == HAL_OK);
}
static bool spi_rx(W25Q_Handle *d, uint8_t *buf, uint16_t len) {
    return (HAL_SPI_Receive(d->hspi, buf, len, 1000) == HAL_OK);
}

/* ===================== STATUS ===================== */
bool w25q_is_busy(W25Q_Handle *d) {
    uint8_t cmd = CMD_READ_SR1;
    uint8_t sr  = 0;

    cs_low(d);
    spi_tx(d, &cmd, 1);
    spi_rx(d, &sr, 1);
    cs_high(d);

    return (sr & SR1_BUSY);
}

bool w25q_wait_ready(W25Q_Handle *d, uint32_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    while (w25q_is_busy(d)) {
        if ((HAL_GetTick() - start) > timeout_ms)
            return false;
        HAL_Delay(1);
    }
    return true;
}

/* ===================== CORE ===================== */
static bool write_enable(W25Q_Handle *d) {
    uint8_t cmd = CMD_WRITE_ENABLE;
    cs_low(d);
    bool ok = spi_tx(d, &cmd, 1);
    cs_high(d);
    return ok;
}

uint32_t w25q_read_jedec_id(W25Q_Handle *d) {
    uint8_t cmd = CMD_JEDEC_ID;
    uint8_t id[3] = {0};

    cs_low(d);
    spi_tx(d, &cmd, 1);
    spi_rx(d, id, 3);
    cs_high(d);

    return ((uint32_t)id[0] << 16) |
           ((uint32_t)id[1] << 8)  |
           ((uint32_t)id[2]);
}

bool w25q_init(W25Q_Handle *d) {
    cs_high(d);
    HAL_Delay(2);

    uint32_t id = w25q_read_jedec_id(d);

    /* Expected for W25Q16JV: EF 40 15 */
    if (id == 0 || id == 0xFFFFFF)
        return false;

    return true;
}

/* ===================== READ ===================== */
bool w25q_read(W25Q_Handle *d, uint32_t addr, uint8_t *buf, uint32_t len) {
    uint8_t hdr[4];

    hdr[0] = CMD_READ_DATA;
    hdr[1] = (addr >> 16) & 0xFF;
    hdr[2] = (addr >> 8)  & 0xFF;
    hdr[3] = addr & 0xFF;

    cs_low(d);
    if (!spi_tx(d, hdr, 4)) { cs_high(d); return false; }
    bool ok = spi_rx(d, buf, len);
    cs_high(d);

    return ok;
}

/* ===================== WRITE ===================== */
static bool page_program(W25Q_Handle *d, uint32_t addr,
                         const uint8_t *buf, uint32_t len)
{
    if (len == 0 || len > W25Q_PAGE_SIZE)
        return false;

    if ((addr % W25Q_PAGE_SIZE) + len > W25Q_PAGE_SIZE)
        return false;

    if (!write_enable(d))
        return false;

    uint8_t hdr[4];
    hdr[0] = CMD_PAGE_PROGRAM;
    hdr[1] = (addr >> 16) & 0xFF;
    hdr[2] = (addr >> 8)  & 0xFF;
    hdr[3] = addr & 0xFF;

    cs_low(d);
    if (!spi_tx(d, hdr, 4)) { cs_high(d); return false; }
    if (!spi_tx(d, buf, len)) { cs_high(d); return false; }
    cs_high(d);

    return w25q_wait_ready(d, 5000);
}

bool w25q_write(W25Q_Handle *d, uint32_t addr,
                const uint8_t *buf, uint32_t len)
{
    while (len) {
        uint32_t page_off = addr % W25Q_PAGE_SIZE;
        uint32_t chunk = W25Q_PAGE_SIZE - page_off;
        if (chunk > len)
            chunk = len;

        if (!page_program(d, addr, buf, chunk))
            return false;

        addr += chunk;
        buf  += chunk;
        len  -= chunk;
    }
    return true;
}

/* ===================== ERASE ===================== */
bool w25q_erase_sector_4k(W25Q_Handle *d, uint32_t addr) {
    addr &= ~(W25Q_SECTOR_SIZE - 1);

    if (!write_enable(d))
        return false;

    uint8_t cmd[4];
    cmd[0] = CMD_SECTOR_ERASE_4K;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8)  & 0xFF;
    cmd[3] = addr & 0xFF;

    cs_low(d);
    bool ok = spi_tx(d, cmd, 4);
    cs_high(d);

    if (!ok)
        return false;

    return w25q_wait_ready(d, 10000);
}
