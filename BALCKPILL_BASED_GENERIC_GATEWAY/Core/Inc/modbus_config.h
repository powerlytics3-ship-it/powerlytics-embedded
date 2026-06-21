#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ---------- Config metadata ---------- */

#define MODBUS_CFG_MAGIC     0x4D424346u   // 'MBCF'
#define MODBUS_CFG_VERSION   2u            // bump because struct layout changed

/* Limits (keep small; can increase later) */
#define MODBUS_MAX_SLAVES            4u
#define MODBUS_MAX_READS_PER_SLAVE   16u

/* String sizes (UUID = 36 chars + null) */
#define MODBUS_UUID_STR_LEN          37u
#define MODBUS_DEVICE_ID_LEN         32u   // fits Mongo id "695fe..." (24) + slack
#define MODBUS_IMEI_LEN              20u

/* ---------- Modbus function types ---------- */

typedef enum {
  MB_FUNC_HOLDING_03 = 3,
  MB_FUNC_INPUT_04   = 4,
} mb_func_t;

/* ---------- One register read definition (readId aligned) ---------- */
typedef struct {
  char     read_id[MODBUS_UUID_STR_LEN];  // UUID string
  uint8_t  func;                          // 3 or 4
  uint16_t start_reg;                     // start address
  uint8_t  qty;                           // number of 16-bit registers: 1/2/4
  uint8_t  reserved0;
} mb_read_item_t;

/* ---------- One slave definition (contains multiple reads) ---------- */
typedef struct {
  char     unique_slave_id[MODBUS_UUID_STR_LEN]; // UUID string from config
  uint8_t  slave_id;                              // numeric modbus address

  // serial (per slave, as per new config)
  uint32_t baud;
  uint8_t  parity;     // 0=None, 1=Even, 2=Odd
  uint8_t  stop_bits;  // 1 or 2
  uint8_t  data_bits;  // typically 8
  uint8_t  reserved1;

  // polling (per slave)
  uint32_t interval_ms;
  uint32_t timeout_ms;
  uint8_t  retries;
  uint8_t  reserved2[3];

  // reads
  uint8_t        read_count;
  uint8_t        reserved3[3];
  mb_read_item_t reads[MODBUS_MAX_READS_PER_SLAVE];

} mb_slave_cfg_t;

/* ---------- Full Modbus config stored in FLASH ---------- */
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t length;
  uint32_t crc32;

  // identifiers from config JSON
  char device_id[MODBUS_DEVICE_ID_LEN];
  char config_id[MODBUS_UUID_STR_LEN];
  char imei[MODBUS_IMEI_LEN];

  // slaves
  uint8_t        slave_count;
  uint8_t        reservedA[3];
  mb_slave_cfg_t slaves[MODBUS_MAX_SLAVES];

} modbus_cfg_t;

/* ---------- API ---------- */

// Fill structure with safe defaults
void modbus_cfg_defaults(modbus_cfg_t *cfg);

// Load config from FLASH (returns false if invalid)
bool modbus_cfg_load(modbus_cfg_t *out);

// Save config to FLASH
bool modbus_cfg_save(const modbus_cfg_t *cfg);
