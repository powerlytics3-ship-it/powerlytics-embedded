#ifndef MODEM_HANDLER_H
#define MODEM_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h> // Required for the new bool parameter

/* Public API */
void     WakeUp_GSM(void);

/**
 * @brief Sends an AT command and waits for a specific response.
 * @param command: The AT command string (e.g. "AT")
 * @param expected: The substring to look for (e.g. "OK")
 * @param timeout: Time to wait in milliseconds
 * @param flush_rx: If true, clears the buffer before sending.
 * Set to false when expecting a URC immediately after a command response.
 */
uint8_t  SIM800_SendATCommand2(const char *command, const char *expected, uint32_t timeout, bool flush_rx);

uint8_t  SendCommandWithRetry(const char *cmd, const char *expected, uint16_t timeout, uint8_t retries);

uint8_t  modem_at_rx_is_active(void);
void     modem_at_rx_feed(const uint8_t *data, uint16_t len);

/* GPRS bring-up with APN from config */
uint8_t  GPRS_Init(const char *apn);

void     GetIMEI(void);
void     WaitForNetworkAndBlinkLED(void);
void     Check_Internet_Service(void);

#ifdef __cplusplus
}
#endif

#endif /* MODEM_HANDLER_H */
