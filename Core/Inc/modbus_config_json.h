#pragma once
#include <stdbool.h>
#include "modbus_config.h"

bool modbus_cfg_from_json(const char *json, modbus_cfg_t *cfg);
