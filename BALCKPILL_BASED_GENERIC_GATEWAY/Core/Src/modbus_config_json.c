#include "modbus_config_json.h"
#include "cJSON.h"
#include <string.h>
#include <stdint.h>

static uint8_t parity_from_str(const char *s)
{
  if (!s) return 0;
  if (strcmp(s, "even") == 0) return 1;
  if (strcmp(s, "odd")  == 0) return 2;
  return 0; // "none" or unknown
}

static uint8_t func_from_str(const char *s)
{
  if (!s) return 0;
  if (strcmp(s, "fc_3") == 0) return 3;
  if (strcmp(s, "fc_4") == 0) return 4;
  return 0;
}

static uint8_t qty_from_bits(int bits)
{
  // bits are 16/32/64 -> qty 1/2/4 registers
  if (bits <= 0) return 0;
  if ((bits % 16) != 0) return 0;
  int q = bits / 16;
  if (q == 1 || q == 2 || q == 4) return (uint8_t)q;
  return 0;
}

static void copy_json_str(char *dst, size_t dst_sz, cJSON *item)
{
  if (!dst || dst_sz == 0) return;
  dst[0] = '\0';
  if (cJSON_IsString(item) && item->valuestring) {
    strncpy(dst, item->valuestring, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
  }
}

bool modbus_cfg_from_json(const char *json, modbus_cfg_t *cfg)
{
  if (!json || !cfg) return false;

  // start with defaults so missing fields won’t break you
  modbus_cfg_defaults(cfg);

  cJSON *root = cJSON_Parse(json);
  if (!root) return false;

  // root identifiers
  copy_json_str(cfg->device_id, sizeof(cfg->device_id),
                cJSON_GetObjectItemCaseSensitive(root, "device_id"));
  copy_json_str(cfg->config_id, sizeof(cfg->config_id),
                cJSON_GetObjectItemCaseSensitive(root, "configId"));
  copy_json_str(cfg->imei, sizeof(cfg->imei),
                cJSON_GetObjectItemCaseSensitive(root, "imei"));

  // modbusSlaves[]
  cJSON *slaves = cJSON_GetObjectItemCaseSensitive(root, "modbusSlaves");
  if (!cJSON_IsArray(slaves)) {
    cJSON_Delete(root);
    return false;
  }

  uint8_t sidx = 0;

  cJSON *s = NULL;
  cJSON_ArrayForEach(s, slaves) {
    if (!cJSON_IsObject(s)) continue;
    if (sidx >= MODBUS_MAX_SLAVES) break;

    mb_slave_cfg_t *sc = &cfg->slaves[sidx];
    memset(sc, 0, sizeof(*sc));

    // unique_slave_id (UUID string)
    copy_json_str(sc->unique_slave_id, sizeof(sc->unique_slave_id),
                  cJSON_GetObjectItemCaseSensitive(s, "unique_slave_id"));

    // numeric slave_id
    cJSON *sid = cJSON_GetObjectItemCaseSensitive(s, "slave_id");
    if (cJSON_IsNumber(sid) && sid->valuedouble >= 0) {
      sc->slave_id = (uint8_t)sid->valuedouble;
    }

    // serial
    cJSON *serial = cJSON_GetObjectItemCaseSensitive(s, "serial");
    if (cJSON_IsObject(serial)) {
      cJSON *baud = cJSON_GetObjectItemCaseSensitive(serial, "baudRate");
      cJSON *db   = cJSON_GetObjectItemCaseSensitive(serial, "dataBits");
      cJSON *sb   = cJSON_GetObjectItemCaseSensitive(serial, "stopBits");
      cJSON *par  = cJSON_GetObjectItemCaseSensitive(serial, "parity");

      if (cJSON_IsNumber(baud) && baud->valuedouble > 0) sc->baud      = (uint32_t)baud->valuedouble;
      if (cJSON_IsNumber(db)   && db->valuedouble > 0)   sc->data_bits = (uint8_t)db->valuedouble;
      if (cJSON_IsNumber(sb))                             sc->stop_bits = ((int)sb->valuedouble == 2) ? 2 : 1;
      if (cJSON_IsString(par) && par->valuestring)       sc->parity    = parity_from_str(par->valuestring);
    }

    // polling
    cJSON *polling = cJSON_GetObjectItemCaseSensitive(s, "polling");
    if (cJSON_IsObject(polling)) {
      cJSON *ival = cJSON_GetObjectItemCaseSensitive(polling, "intervalMs");
      cJSON *tout = cJSON_GetObjectItemCaseSensitive(polling, "timeoutMs");
      cJSON *ret  = cJSON_GetObjectItemCaseSensitive(polling, "retries");

      if (cJSON_IsNumber(ival) && ival->valuedouble >= 0) sc->interval_ms = (uint32_t)ival->valuedouble;
      if (cJSON_IsNumber(tout) && tout->valuedouble >= 0) sc->timeout_ms  = (uint32_t)tout->valuedouble;
      if (cJSON_IsNumber(ret)  && ret->valuedouble >= 0)  sc->retries     = (uint8_t)ret->valuedouble;
    }

    // registers[]
    cJSON *regs = cJSON_GetObjectItemCaseSensitive(s, "registers");
    if (!cJSON_IsArray(regs)) {
      // no registers => skip this slave
      continue;
    }

    uint8_t ridx = 0;

    cJSON *r = NULL;
    cJSON_ArrayForEach(r, regs) {
      if (!cJSON_IsObject(r)) continue;
      if (ridx >= MODBUS_MAX_READS_PER_SLAVE) break;

      mb_read_item_t *rc = &sc->reads[ridx];
      memset(rc, 0, sizeof(*rc));

      copy_json_str(rc->read_id, sizeof(rc->read_id),
                    cJSON_GetObjectItemCaseSensitive(r, "readId"));

      cJSON *func = cJSON_GetObjectItemCaseSensitive(r, "func");
      if (cJSON_IsString(func) && func->valuestring) {
        rc->func = func_from_str(func->valuestring);
      }

      cJSON *start = cJSON_GetObjectItemCaseSensitive(r, "start");
      if (cJSON_IsNumber(start) && start->valuedouble >= 0) {
        rc->start_reg = (uint16_t)start->valuedouble;
      }

      cJSON *bits = cJSON_GetObjectItemCaseSensitive(r, "bits");
      if (cJSON_IsNumber(bits)) {
        rc->qty = qty_from_bits((int)bits->valuedouble); // 1/2/4
      }

      // minimal validation: must have readId, func (3/4), qty (1/2/4)
      if (rc->read_id[0] == '\0') continue;
      if (!(rc->func == 3 || rc->func == 4)) continue;
      if (!(rc->qty == 1 || rc->qty == 2 || rc->qty == 4)) continue;

      ridx++;
    }

    // Accept slave only if it has required ids + at least 1 read
    if (sc->unique_slave_id[0] == '\0') continue;
    if (sc->slave_id == 0) continue; // keep strict for now
    if (ridx == 0) continue;

    sc->read_count = ridx;
    sidx++;
  }

  cfg->slave_count = sidx;

  cJSON_Delete(root);
  return (cfg->slave_count > 0);
}
