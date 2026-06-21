#pragma once
#include "modbus_config.h"
#include <stdint.h>

uint8_t cfg_is_apply_pending(void);
const modbus_cfg_t* cfg_get_pending(void);
const char* cfg_get_pending_config_id(void);
void cfg_clear_pending(void);

/* called by downlink handler */
uint8_t cfg_stage_pending(const char *cfg_min_json,
                          const char *config_id);
