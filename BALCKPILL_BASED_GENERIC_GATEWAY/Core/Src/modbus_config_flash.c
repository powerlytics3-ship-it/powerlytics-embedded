#include "modbus_config.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ---- Choose Flash location (edit if your flash size differs) ---- */
#ifndef MODBUS_CFG_FLASH_ADDR
#define MODBUS_CFG_FLASH_ADDR   (0x08060000u)      // adjust to your MCU flash map
#endif

#ifndef MODBUS_CFG_FLASH_SECTOR
#define MODBUS_CFG_FLASH_SECTOR FLASH_SECTOR_7
#endif

/* ---------- CRC32 software (simple, reliable) ---------- */
static uint32_t crc32_sw(const void *data, uint32_t len)
{
  const uint8_t *p = (const uint8_t*)data;
  uint32_t crc = 0xFFFFFFFFu;

  for (uint32_t i = 0; i < len; i++) {
    crc ^= p[i];
    for (int b = 0; b < 8; b++) {
      uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

/* ---------- Defaults ---------- */
void modbus_cfg_defaults(modbus_cfg_t *cfg)
{
  if (!cfg) return;
  memset(cfg, 0, sizeof(*cfg));

  cfg->magic   = MODBUS_CFG_MAGIC;
  cfg->version = MODBUS_CFG_VERSION;
  cfg->length  = sizeof(modbus_cfg_t);

  // identifiers empty by default
  cfg->device_id[0] = '\0';
  cfg->config_id[0] = '\0';
  cfg->imei[0]      = '\0';

  // no slaves by default
  cfg->slave_count = 0;

  // (optional) you can preset per-slave defaults here, but not required
}

/* ---------- Validate flash struct ---------- */
static bool cfg_is_valid(const modbus_cfg_t *cfg)
{
  if (cfg->magic != MODBUS_CFG_MAGIC) return false;
  if (cfg->version != MODBUS_CFG_VERSION) return false;
  if (cfg->length != sizeof(modbus_cfg_t)) return false;

  if (cfg->slave_count > MODBUS_MAX_SLAVES) return false;

  // validate read counts (defensive)
  for (uint8_t i = 0; i < cfg->slave_count; i++) {
    if (cfg->slaves[i].read_count > MODBUS_MAX_READS_PER_SLAVE) return false;
  }

  modbus_cfg_t tmp;
  memcpy(&tmp, cfg, sizeof(tmp));
  uint32_t stored = tmp.crc32;
  tmp.crc32 = 0;

  uint32_t calc = crc32_sw(&tmp, sizeof(tmp));
  return (stored == calc);
}

/* ---------- Load from flash ---------- */
bool modbus_cfg_load(modbus_cfg_t *out)
{
  if (!out) return false;

  const modbus_cfg_t *in_flash = (const modbus_cfg_t*)MODBUS_CFG_FLASH_ADDR;

  if (cfg_is_valid(in_flash)) {
    memcpy(out, in_flash, sizeof(*out));
    return true;
  }

  modbus_cfg_defaults(out);
  return false;
}

/* ---------- Erase one sector ---------- */
static HAL_StatusTypeDef flash_erase_sector(uint32_t sector)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0;

  erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  erase.Sector       = sector;
  erase.NbSectors    = 1;

  return HAL_FLASHEx_Erase(&erase, &sector_error);
}

/* ---------- Save to flash ---------- */
bool modbus_cfg_save(const modbus_cfg_t *cfg)
{
  if (!cfg) return false;

  modbus_cfg_t w;
  memcpy(&w, cfg, sizeof(w));

  // enforce header fields
  w.magic   = MODBUS_CFG_MAGIC;
  w.version = MODBUS_CFG_VERSION;
  w.length  = sizeof(modbus_cfg_t);

  // clamp counts (defensive)
  if (w.slave_count > MODBUS_MAX_SLAVES) w.slave_count = MODBUS_MAX_SLAVES;
  for (uint8_t i = 0; i < w.slave_count; i++) {
    if (w.slaves[i].read_count > MODBUS_MAX_READS_PER_SLAVE) {
      w.slaves[i].read_count = MODBUS_MAX_READS_PER_SLAVE;
    }
  }

  // compute crc
  w.crc32 = 0;
  w.crc32 = crc32_sw(&w, sizeof(w));

  const uint32_t *src = (const uint32_t*)&w;
  uint32_t words = sizeof(w) / 4;

  HAL_FLASH_Unlock();

  if (flash_erase_sector(MODBUS_CFG_FLASH_SECTOR) != HAL_OK) {
    HAL_FLASH_Lock();
    return false;
  }

  for (uint32_t i = 0; i < words; i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          MODBUS_CFG_FLASH_ADDR + (i * 4),
                          src[i]) != HAL_OK) {
      HAL_FLASH_Lock();
      return false;
    }
  }

  HAL_FLASH_Lock();

  // verify
  if (memcmp((void*)MODBUS_CFG_FLASH_ADDR, &w, sizeof(w)) != 0) {
    return false;
  }

  return true;
}
