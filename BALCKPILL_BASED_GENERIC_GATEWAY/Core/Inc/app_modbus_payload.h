#ifndef APP_MODBUS_PAYLOAD_H
#define APP_MODBUS_PAYLOAD_H

#include "stm32f4xx_hal.h"
#include "modbus_rtu_master.h"
#include "modbus_config.h"

void app_modbus_payload_set_cfg(const modbus_cfg_t *cfg);

void app_modbus_payload_init(mbm_t *mbm);
const char* app_build_payload_json_once(void);

#endif
