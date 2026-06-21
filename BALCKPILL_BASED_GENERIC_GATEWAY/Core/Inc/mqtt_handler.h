#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Public API */
uint8_t MQTT_Connect(const char *host, int port, const char *user, const char *pass);
uint8_t MQTT_Subscribe(const char *topic);
uint8_t MQTT_Publish(const char *topic, const char *payload);
char* MQTT_Subscriber_Run(void);
void    handle_downlink_cjson(const char *payload);
void    Prepare_MQTT_Topics(void);
uint8_t MQTT_DrainQueuedStatus(uint8_t max_msgs);
uint8_t MQTT_HasPendingConfig(void);
uint8_t MQTT_ApplyPendingConfig_AndReset(void);

#endif
