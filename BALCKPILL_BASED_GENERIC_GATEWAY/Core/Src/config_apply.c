#include "config_apply.h"
#include "FreeRTOS.h"
#include <string.h>

static struct {
    uint8_t pending;
    char configId[64];
    modbus_cfg_t cfg;
} s_cfg;

uint8_t cfg_stage_pending(const char *cfg_min_json,
                          const char *config_id)
{
    if (!cfg_min_json || !config_id) return 0;

    memset(&s_cfg, 0, sizeof(s_cfg));

    if (!modbus_cfg_from_json(cfg_min_json, &s_cfg.cfg)) {
        return 0;
    }

    snprintf(s_cfg.configId, sizeof(s_cfg.configId), "%s", config_id);
    s_cfg.pending = 1;
    return 1;
}

uint8_t cfg_is_apply_pending(void)
{
    return s_cfg.pending;
}

const modbus_cfg_t* cfg_get_pending(void)
{
    return s_cfg.pending ? &s_cfg.cfg : NULL;
}

const char* cfg_get_pending_config_id(void)
{
    return s_cfg.pending ? s_cfg.configId : NULL;
}

void cfg_clear_pending(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
}
