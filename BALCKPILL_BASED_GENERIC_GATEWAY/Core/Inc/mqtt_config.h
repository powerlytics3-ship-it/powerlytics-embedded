#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Handle a config message received via MQTT.
 * - payload: pointer to char buffer (NOT necessarily NUL-terminated)
 * - len    : number of valid bytes in payload
 * Returns true on success (validated -> flashed -> readback OK).
 */
bool MQTT_Config_HandleMessage(const char *payload, size_t len);

/* Copy the last good config (read from flash) into out_buf.
 * Returns true if a valid config existed and was copied.
 * out_len (optional) returns the number of bytes copied (not counting NUL).
 */
bool MQTT_Config_GetLast(char *out_buf, size_t out_buf_size, size_t *out_len);

#endif /* MQTT_CONFIG_H */
