/* mqtt_handler.c
 * * - Re-integrated original TLS/SNI logic for stability.
 * - Updated SIM800_SendATCommand2 calls to include 'flush_rx' parameter.
 * - Fixed asynchronous MQTT connection by splitting Command and URC wait.
 */

#include "mqtt_handler.h"
#include "main.h"
#include "debug_uart.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h> // Required for bool

#include "cJSON.h"
#include "sha256.h"
#include "modbus_config_json.h"
#include "uart2_rx_engine.h"

#include "config.h"
#include "mqtt_config.h"

/* Local helper: trim trailing CR/LF and leading CR/LF */
static void trim_crlf(char *s)
{
    if (!s) return;
    /* trim leading */
    while (*s == '\r' || *s == '\n') {
        memmove(s, s + 1, strlen(s));
    }
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\r' || s[n-1] == '\n')) {
        s[n-1] = 0;
        n--;
    }
}

/* ===== Externals ===== */
extern uart2_rx_t g_u2rx;
extern char imei[20];
extern char topic_d2c_telemetry[64];
extern char topic_c2d_commands[64];
extern char topic_d2c_message[64];
extern char topic_d2c_logs[64];
extern uint8_t pump_state;

uint32_t lastMQTTSuccessTime = 0;

/* Updated Prototypes */
uint8_t SIM800_SendATCommand2(const char *command, const char *expected, uint32_t timeout, bool flush_rx);
uint8_t SendCommandWithRetry(const char *cmd, const char *expected, uint16_t timeout, uint8_t retries);

#define MQTT_MAX_PAYLOAD (1024U * 4U)

/* =========================================================
   Topics
   ========================================================= */
void Prepare_MQTT_Topics(void)
{
    snprintf(topic_d2c_telemetry, sizeof(topic_d2c_telemetry), "d2c/%s/telemetry", "696bf997ecbc1c803c08fc2a");
    snprintf(topic_c2d_commands,    sizeof(topic_c2d_commands),    "c2d/%s/commands","696bf997ecbc1c803c08fc2a");
    snprintf(topic_d2c_message,    sizeof(topic_d2c_message),    "d2c/%s/message","696bf997ecbc1c803c08fc2a");
    snprintf(topic_d2c_logs,    sizeof(topic_d2c_logs),    "d2c/%s/logs","696bf997ecbc1c803c08fc2a");
}

/* =========================================================
   FIXED MQTT CONNECT (Old Logic + New Engine)
   ========================================================= */
uint8_t MQTT_Connect(const char *host, int port, const char *user, const char *pass)
{
    char cmd[180];

    /* Fallback to config.h if arguments are NULL */
    const char *host_u = (host && host[0]) ? host : MQTT_HOST_URI;
    int port_u         = (port > 0) ? port : MQTT_PORT;
    const char *user_u = (user && user[0]) ? user : MQTT_USERNAME;
    const char *pass_u = (pass && pass[0]) ? pass : MQTT_PASSWORD;

    print("MQTT: Starting Connection (TLS)...\r\n");

    /* 1. Reset Session (flush_rx = true to clear previous noise) */
    (void)SIM800_SendATCommand2("AT+CMQTTDISC=0,120", "OK", 300, true);
    (void)SIM800_SendATCommand2("AT+CMQTTREL=0", "OK", 200, true);
    (void)SIM800_SendATCommand2("AT+CMQTTSTOP", "OK", 200, true);

    /* 2. TLS Setup (Using your working "No-Verify" logic) */
    if (!SendCommandWithRetry("AT+CSSLCFG=\"sslversion\",0,4", "OK", 5000, 3)) goto fail;
    if (!SendCommandWithRetry("AT+CSSLCFG=\"authmode\",0,0", "OK", 5000, 3)) goto fail;

    /* SNI Setup - Crucial for cloud brokers like HiveMQ */
    if (!SendCommandWithRetry("AT+CSSLCFG=\"enableSNI\",0,1", "OK", 5000, 3)) {
        print("WARN: enableSNI not accepted; continuing...\r\n");
    }

    /* 3. Start MQTT Service */
    if (!SendCommandWithRetry("AT+CMQTTSTART", "OK", 1000, 3)) goto fail;

    /* 4. Acquire Client (trailing ,1 for secure) */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"STM32_%s\",1", imei);
    if (!SendCommandWithRetry(cmd, "OK", 3000, 3)) goto fail;

    /* 5. Bind MQTT client 0 -> TLS context 0 */
    if (!SendCommandWithRetry("AT+CMQTTSSLCFG=0,0", "OK", 1000, 3)) goto fail;

    /* 6. Connect over TLS */
    snprintf(cmd, sizeof(cmd),
             "AT+CMQTTCONNECT=0,\"%s:%d\",300,1,\"%s\",\"%s\"",
             host_u, port_u, user_u, pass_u);

    print("Connecting to Broker: %s:%d\r\n", host_u, port_u);

    /* --- ASYNC FIX ---
     * Step A: Send the command and wait for the modem's immediate "OK".
     * We use flush_rx=true here to ensure the buffer is clean before sending.
     */
    if (!SIM800_SendATCommand2(cmd, "OK", 5000, true)) {
        print("MQTT CONNECT command rejected by modem\r\n");
        goto fail;
    }

    /* Step B: Wait for the Cloud URC (+CMQTTCONNECT: 0,0).
     * We use u2rx_wait_for because the success message might take several seconds
     * while the TLS handshake completes.
     */
    print("Waiting for Cloud Ack (TLS Handshake)...\r\n");
    if (!u2rx_wait_for(&g_u2rx, "+CMQTTCONNECT: 0,0", 15000)) {
        print("MQTT Connection Failed or Timed Out (No URC seen)\r\n");
        goto fail;
    }

    print("MQTT Connected (TLS, no-verify)\r\n");
    lastMQTTSuccessTime = HAL_GetTick();
    return 1;

fail:
    print("MQTT Connect FAIL -> System Reset\r\n");
    osDelay(500);
    NVIC_SystemReset();
    return 0;
}




/* =========================================================
   Standard Operations (Updated Signatures)
   ========================================================= */

uint8_t MQTT_Subscribe(const char *topic)
{
    const char *topic_u = (topic && topic[0]) ? topic : topic_c2d_commands;
    char cmd[80];

    snprintf(cmd, sizeof(cmd), "AT+CMQTTSUBTOPIC=0,%d,1", (int)strlen(topic_u));
    if (!SendCommandWithRetry(cmd, ">", 800, 3)) goto fail;
    if (!SendCommandWithRetry(topic_u, "OK", 800, 3)) goto fail;

    if (!SendCommandWithRetry("AT+CMQTTSUB=0", "+CMQTTSUB: 0,0", 5000, 3)) goto fail;

    print("Subscribed OK\r\n");
    return 1;

fail:
    print("Subscribe FAIL -> reset\r\n");
    HAL_Delay(500);
    NVIC_SystemReset();
    return 0;
}

uint8_t MQTT_Publish(const char *topic, const char *payload)
{
    if (!topic || !topic[0] || !payload) return 0;
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%d", (int)strlen(topic));
    if (!SendCommandWithRetry(cmd, ">", 800, 2)) return 0;
    if (!SendCommandWithRetry(topic, "OK", 800, 2)) return 0;

    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%d", (int)strlen(payload));
    if (!SendCommandWithRetry(cmd, ">", 800, 2)) return 0;
    if (!SendCommandWithRetry(payload, "OK", 800, 2)) return 0;

    if (!SendCommandWithRetry("AT+CMQTTPUB=0,1,60", "OK", 6000, 2))
    return 0;

    lastMQTTSuccessTime = HAL_GetTick();
    return 1;
}

/* Rest of MQTT functions (MQTT_Maintain, Subscriber, etc.) remain as previously updated... */

void MQTT_Maintain(uint32_t keepalive_ms, const char *topic, const char *ping_payload)
{
    if (keepalive_ms == 0) return;

    uint32_t now = HAL_GetTick();
    if ((now - lastMQTTSuccessTime) > keepalive_ms) {
        const char *t = (topic && topic[0]) ? topic : topic_d2c_telemetry;
        const char *p = (ping_payload && ping_payload[0]) ? ping_payload : "{\"message\":\"ping\"}";
        (void)MQTT_Publish(t, p);
    }
}

uint8_t mqttPublish(const char *message)
{
    return MQTT_Publish(topic_d2c_telemetry, message);
}

void mqttPublishf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    (void)mqttPublish(buf);
}

/* =========================================================
   Subscriber Logic (UNCHANGED)
   ========================================================= */

static int parse_reported_len(const char *line)
{
    const char *last = strrchr(line, ',');
    if (!last) return -1;
    int n = 0;
    last++;
    while (*last == ' ') last++;
    while (*last >= '0' && *last <= '9') {
        n = (n * 10) + (*last - '0');
        last++;
    }
    return (n > 0) ? n : -1;
}

static int u2_read_exact(uint8_t *dst, int want, uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    int got = 0;
    while (got < want && (HAL_GetTick() - t0) < timeout_ms) {
        size_t avail = u2rx_available(&g_u2rx);
        if (avail == 0) {
            (void)osSemaphoreAcquire(g_u2rx.rxSem, 50);
            continue;
        }
        size_t take = avail;
        if (take > (size_t)(want - got)) take = (size_t)(want - got);
        got += (int)u2rx_read(&g_u2rx, dst + got, take);
    }
    return got;
}

char* MQTT_Subscriber_Run(void)
{
    static char payload[MQTT_MAX_PAYLOAD];

    // 1. Wait for the header (don't clear buffer yet)
    if (!u2rx_wait_for(&g_u2rx, "+CMQTTRXPAYLOAD:", 100)) return NULL;

    // 2. Read ONLY the header line to get the length
    char line[128];
    int n = u2rx_read_line(&g_u2rx, line, sizeof(line), 500);
    if (n <= 0) return NULL;

    // 3. Extract the exact length (e.g., 545 from "+CMQTTRXPAYLOAD: 0,545")
    int reported_len = parse_reported_len(line);
    if (reported_len <= 0) return NULL;

    // 4. THE FIX: Read the EXACT number of bytes
    // Do NOT use read_line here. Newlines in JSON will break it.
    int got = u2_read_exact((uint8_t*)payload, reported_len, 5000);
    payload[got] = 0; // Manual null termination

    // 5. Check for footer
    (void)u2rx_wait_for(&g_u2rx, "+CMQTTRXEND", 500);

    print("[MQTT RX] Total Expected: %d, Actually Read: %d\r\n", reported_len, got);
    return (got > 0) ? payload : NULL;
}

//void handle_downlink_cjson(const char *payload_str)
//{
//    if (!payload_str || !payload_str[0]) return;
//
//    char message[256];
//
//    cJSON *root = cJSON_Parse(payload_str);
//    if (!root) {
//        print("CFG: JSON parse failed\r\n");
//        strcpy(message,
//               "{\"status\":\"error\",\"configId\":\"unknown\",\"message\":\"invalid_json_format\"}");
//        MQTT_Publish(topic_d2c_message, message);
//        return;
//    }
//
//    cJSON *msg = cJSON_GetObjectItem(root, "message");
//    if (!cJSON_IsString(msg) || strcmp(msg->valuestring, "config") != 0) {
//        cJSON_Delete(root);
//        return;
//    }
//
//    cJSON *cfgId = cJSON_GetObjectItem(root, "configId");
//    cJSON *hash  = cJSON_GetObjectItem(root, "hash");
//    cJSON *cfg   = cJSON_GetObjectItem(root, "config");
//
//    /* Keep dynamic configId */
//    const char *id_str = (cJSON_IsString(cfgId)) ? cfgId->valuestring : "missing";
//
//    if (strcmp(id_str, "missing") == 0) {
//        strcpy(message,
//               "{\"status\":\"error\",\"configId\":\"missing\",\"message\":\"missing_configId\"}");
//        MQTT_Publish(topic_d2c_message, message);
//        cJSON_Delete(root);
//        return;
//    }
//
//    if (!cJSON_IsString(hash) || !cJSON_IsObject(cfg)) {
//        snprintf(message, sizeof(message),
//                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"missing_data_fields\"}",
//                 id_str);
//        MQTT_Publish(topic_d2c_message, message);
//        cJSON_Delete(root);
//        return;
//    }
//
//    char *cfg_min = cJSON_PrintUnformatted(cfg);
//    if (!cfg_min) {
//        snprintf(message, sizeof(message),
//                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"memory_allocation_error\"}",
//                 id_str);
//        MQTT_Publish(topic_d2c_message, message);
//        cJSON_Delete(root);
//        return;
//    }
//
//    char hash_calc[65] = {0};
//    sha256_hex((const uint8_t*)cfg_min, strlen(cfg_min), hash_calc);
//
//    print("Calculated Hash: %s\n", hash_calc); // Print the calculated hash
//    print("Provided Hash:   %s\n", hash->valuestring); // Print the provided hash
//
//    if (strcmp(hash->valuestring, hash_calc) != 0) {
//        snprintf(message, sizeof(message),
//                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"hash_mismatch\"}",
//                 id_str);
//        MQTT_Publish(topic_d2c_message, message);
//        cJSON_free(cfg_min);
//        cJSON_Delete(root);
//        return;
//    }
//
//    modbus_cfg_t new_cfg;
//    memset(&new_cfg, 0, sizeof(new_cfg));
//
//    if (!modbus_cfg_from_json(cfg_min, &new_cfg)) {
//        snprintf(message, sizeof(message),
//                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"parse_to_struct_failed\"}",
//                 id_str);
//        MQTT_Publish(topic_d2c_message, message);
//        cJSON_free(cfg_min);
//        cJSON_Delete(root);
//        return;
//    }
//
//    if (!modbus_cfg_save(&new_cfg)) {
//        snprintf(message, sizeof(message),
//                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"flash_write_error\"}",
//                 id_str);
//        MQTT_Publish(topic_d2c_message, message);
//        cJSON_free(cfg_min);
//        cJSON_Delete(root);
//        return;
//    }
//
//    /* SUCCESS */
//    snprintf(message, sizeof(message),
//             "{\"status\":\"applied\",\"configId\":\"%s\",\"message\":\"saved_config\"}",
//             id_str);
//    MQTT_Publish(topic_d2c_message, message);
//
//    cJSON_free(cfg_min);
//    cJSON_Delete(root);
//
//    osDelay(2000);
//    print("CFG: Success. Rebooting...\r\n");
//    NVIC_SystemReset();
//}
static void log_hex(const char *label, const char *str) {
    print("%s (Hex): ", label);
    for (size_t i = 0; i < strlen(str); i++) {
        print("%02X ", (unsigned char)str[i]);
    }
    print("\n");
}

void handle_downlink_cjson(const char *payload_str)
{
    if (!payload_str || !payload_str[0]) return;

    char message[256];

    cJSON *root = cJSON_Parse(payload_str);
    if (!root) {
        print("CFG: JSON parse failed\r\n");
        strcpy(message,
               "{\"status\":\"error\",\"configId\":\"unknown\",\"message\":\"invalid_json_format\"}");
        MQTT_Publish(topic_d2c_message, message);
        return;
    }

    cJSON *msg = cJSON_GetObjectItem(root, "message");
    if (!cJSON_IsString(msg) || strcmp(msg->valuestring, "config") != 0) {
        cJSON_Delete(root);
        return;
    }

    cJSON *cfgId = cJSON_GetObjectItem(root, "configId");
    cJSON *hash  = cJSON_GetObjectItem(root, "hash");
    cJSON *cfg   = cJSON_GetObjectItem(root, "config");

    /* Keep dynamic configId */
    const char *id_str = (cJSON_IsString(cfgId)) ? cfgId->valuestring : "missing";

    if (strcmp(id_str, "missing") == 0) {
        strcpy(message,
               "{\"status\":\"error\",\"configId\":\"missing\",\"message\":\"missing_configId\"}");
        MQTT_Publish(topic_d2c_message, message);
        cJSON_Delete(root);
        return;
    }

    if (!cJSON_IsString(hash) || !cJSON_IsObject(cfg)) {
        snprintf(message, sizeof(message),
                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"missing_data_fields\"}",
                 id_str);
        MQTT_Publish(topic_d2c_message, message);
        cJSON_Delete(root);
        return;
    }

    char *cfg_min = cJSON_PrintUnformatted(cfg);
    if (!cfg_min) {
        snprintf(message, sizeof(message),
                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"memory_allocation_error\"}",
                 id_str);
        MQTT_Publish(topic_d2c_message, message);
        cJSON_Delete(root);
        return;
    }

    // Normalize JSON by re-parsing and re-printing to ensure consistent formatting
    cJSON *normalized_cfg = cJSON_Parse(cfg_min);
    if (!normalized_cfg) {
        snprintf(message, sizeof(message),
                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"json_normalization_failed\"}",
                 id_str);
        MQTT_Publish(topic_d2c_message, message);
        cJSON_free(cfg_min);
        cJSON_Delete(root);
        return;
    }

    char *normalized_cfg_min = cJSON_PrintUnformatted(normalized_cfg);
    cJSON_Delete(normalized_cfg);
    if (!normalized_cfg_min) {
        snprintf(message, sizeof(message),
                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"memory_allocation_error\"}",
                 id_str);
        MQTT_Publish(topic_d2c_message, message);
        cJSON_free(cfg_min);
        cJSON_Delete(root);
        return;
    }

    // Log hex representation of JSON strings
//    log_hex("Raw JSON", cfg_min);
//    log_hex("Normalized JSON", normalized_cfg_min);

    char hash_calc[65] = {0};
    sha256_hex((const uint8_t *)normalized_cfg_min, strlen(normalized_cfg_min), hash_calc);

    print("Calculated Hash: %s\n", hash_calc); // Debugging: Print the calculated hash
    print("Provided Hash:   %s\n", hash->valuestring); // Debugging: Print the provided hash

    if (strcmp(hash->valuestring, hash_calc) != 0) {
        snprintf(message, sizeof(message),
                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"hash_mismatch\"}",
                 id_str);
        MQTT_Publish(topic_d2c_message, message);
        cJSON_free(cfg_min);
        free(normalized_cfg_min);
        cJSON_Delete(root);
        return;
    }

    modbus_cfg_t new_cfg;
    memset(&new_cfg, 0, sizeof(new_cfg));

    if (!modbus_cfg_from_json(normalized_cfg_min, &new_cfg)) {
        snprintf(message, sizeof(message),
                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"parse_to_struct_failed\"}",
                 id_str);
        MQTT_Publish(topic_d2c_message, message);
        cJSON_free(cfg_min);
        free(normalized_cfg_min);
        cJSON_Delete(root);
        return;
    }

    if (!modbus_cfg_save(&new_cfg)) {
        snprintf(message, sizeof(message),
                 "{\"status\":\"error\",\"configId\":\"%s\",\"message\":\"flash_write_error\"}",
                 id_str);
        MQTT_Publish(topic_d2c_message, message);
        cJSON_free(cfg_min);
        free(normalized_cfg_min);
        cJSON_Delete(root);
        return;
    }

    /* SUCCESS */
    snprintf(message, sizeof(message),
             "{\"status\":\"applied\",\"configId\":\"%s\",\"message\":\"saved_config\"}",
             id_str);
    MQTT_Publish(topic_d2c_message, message);

    cJSON_free(cfg_min);
    free(normalized_cfg_min);
    cJSON_Delete(root);

    osDelay(2000);
    print("CFG: Success. Rebooting...\r\n");
    NVIC_SystemReset();
}
