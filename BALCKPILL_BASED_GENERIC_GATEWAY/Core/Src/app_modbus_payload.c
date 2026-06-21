#include "app_modbus_payload.h"
#include "app_board_io.h"
#include "debug_uart.h"
#include "rtc_manager.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "modbus_config.h"
#include "modbus_rtu_master.h"   // for mbm_read_* prototypes

static modbus_cfg_t s_cfg;
static uint8_t s_cfg_valid = 0;

/* Static refs */
static mbm_t *s_mbm = NULL;

/* Buffers (avoid stack overflow) */
#define MB_JSON_MAX   2048
#define MB_REG_MAX    125
static char     g_payload_json[MB_JSON_MAX];
static uint16_t g_mb_regs[MB_REG_MAX];

/* ----- json builder ----- */
static int jb_append(char *dst, int max, int *idx, const char *fmt, ...)
{
  if (*idx >= max) return -1;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(&dst[*idx], (size_t)(max - *idx), fmt, ap);
  va_end(ap);
  if (n < 0) return -1;
  if (n >= (max - *idx)) { *idx = max; return -1; }
  *idx += n;
  return 0;
}

/* ----- public init ----- */
void app_modbus_payload_init(mbm_t *mbm)
{
  s_mbm = mbm;
}

/* ---- config setter ---- */
void app_modbus_payload_set_cfg(const modbus_cfg_t *cfg)
{
  if (!cfg) return;
  s_cfg = *cfg;          // struct copy
  s_cfg_valid = 1;
}

/* =========================================================
   JSON helpers (new expected format)
   ========================================================= */

static void json_values_add_di_2(char *json, int max, int *idx, uint8_t *first_kv)
{
  uint8_t d[8] = {0};
  app_di_read_all(d);

  // only DI_1, DI_2 (as per your requirement)
  for (int i = 0; i < 8; i++) {
    if (!(*first_kv)) jb_append(json, max, idx, ",");
    *first_kv = 0;
    jb_append(json, max, idx, "\"DI_%d\":%u", i + 1, (unsigned)d[i]);
  }
}

static void json_values_add_ai_2_mv(char *json, int max, int *idx, uint8_t *first_kv)
{
  uint32_t mv[10] = {0};
  app_adc_read_all_mv(mv);

  // only AI_1, AI_2 (as per your requirement)
  for (int i = 0; i < 10; i++) {
    if (!(*first_kv)) jb_append(json, max, idx, ",");
    *first_kv = 0;
    jb_append(json, max, idx, "\"AI_%d\":%lu", i + 1, (unsigned long)mv[i]);
  }
}

/* One read execution (no optimization, 1 readId = 1 request) */
static HAL_StatusTypeDef mb_read_by_func(uint8_t func,
                                        uint8_t slave_id,
                                        uint16_t start,
                                        uint16_t qty,
                                        uint32_t timeout_ms,
                                        uint16_t *out_regs)
{
  if (!s_mbm) return HAL_ERROR;

  if (func == MB_FUNC_HOLDING_03 || func == 3) {
    return mbm_read_holding_03(s_mbm, slave_id, start, qty, timeout_ms, out_regs);
  } else if (func == MB_FUNC_INPUT_04 || func == 4) {
    return mbm_read_input_04(s_mbm, slave_id, start, qty, timeout_ms, out_regs);
  }
  return HAL_ERROR;
}

/* Add MI_1 populated from config */
static void json_values_add_mi_1(char *json, int max, int *idx, uint8_t *first_kv)
{
  if (!(*first_kv)) jb_append(json, max, idx, ",");
  *first_kv = 0;

  jb_append(json, max, idx, "\"MI_1\":[");

  if (!s_cfg_valid || s_cfg.slave_count == 0) {
    jb_append(json, max, idx, "]");
    return;
  }

  for (uint8_t si = 0; si < s_cfg.slave_count; si++) {
    const mb_slave_cfg_t *sc = &s_cfg.slaves[si];

    if (si > 0) jb_append(json, max, idx, ",");

    // In your expected response field name is "slave_id" but value is UUID
    jb_append(json, max, idx, "{");
    jb_append(json, max, idx, "\"slave_id\":\"%s\",", sc->unique_slave_id);
    jb_append(json, max, idx, "\"registers\":[");

    for (uint8_t ri = 0; ri < sc->read_count; ri++) {
      const mb_read_item_t *rc = &sc->reads[ri];

      if (ri > 0) jb_append(json, max, idx, ",");

      // execute read (raw words only)
      uint16_t qty = rc->qty;
      if (qty == 0 || qty > MB_REG_MAX) qty = 0;

      HAL_StatusTypeDef st = HAL_ERROR;
      if (qty > 0) {
        st = mb_read_by_func(rc->func,
                             sc->slave_id,
                             rc->start_reg,
                             qty,
                             sc->timeout_ms,
                             g_mb_regs);
      }

      jb_append(json, max, idx, "{");
      jb_append(json, max, idx, "\"readId\":\"%s\",", rc->read_id);

      // "value":[...]
      jb_append(json, max, idx, "\"value\":[");
      if (st == HAL_OK && qty > 0) {
        for (uint16_t k = 0; k < qty; k++) {
          if (k > 0) jb_append(json, max, idx, ",");
          jb_append(json, max, idx, "%u", (unsigned)g_mb_regs[k]);
        }
      }
      jb_append(json, max, idx, "]");

      jb_append(json, max, idx, "}");
    }

    jb_append(json, max, idx, "]");
    jb_append(json, max, idx, "}");
  }

  jb_append(json, max, idx, "]");
}

/* ----- build full payload ----- */
const char* app_build_payload_json_once(void)
{
  char ts[40];
  rtc_get_datetime_strings(ts, sizeof(ts));

  int idx = 0;
  char *json = g_payload_json;

  jb_append(json, MB_JSON_MAX, &idx, "{");

  // deviceId: use from config if available, else fallback empty
  const char *dev_id = (s_cfg_valid && s_cfg.device_id[0]) ? s_cfg.device_id : "";
  jb_append(json, MB_JSON_MAX, &idx, "\"deviceId\":\"%s\",", dev_id);

  // ts
  jb_append(json, MB_JSON_MAX, &idx, "\"ts\":\"%s\",", ts);

  // values object
  jb_append(json, MB_JSON_MAX, &idx, "\"values\":{");

  uint8_t first_kv = 1;

  // DI_1..DI_2 only
  json_values_add_di_2(json, MB_JSON_MAX, &idx, &first_kv);

  // AI_1..AI_2 only (mv)
  json_values_add_ai_2_mv(json, MB_JSON_MAX, &idx, &first_kv);

  // MI_1 populated
  json_values_add_mi_1(json, MB_JSON_MAX, &idx, &first_kv);

  jb_append(json, MB_JSON_MAX, &idx, "}"); // end values
  jb_append(json, MB_JSON_MAX, &idx, "}"); // end root

  if (idx >= MB_JSON_MAX) json[MB_JSON_MAX - 1] = '\0';
  else json[idx] = '\0';

  print("%s\r\n", json);
  return json;
}
