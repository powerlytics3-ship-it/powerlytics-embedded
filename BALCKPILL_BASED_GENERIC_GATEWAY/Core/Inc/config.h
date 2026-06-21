#ifndef CONFIG_H
#define CONFIG_H

// --- Network/MQTT configuration (kept identical to your working setup) ---
#define APN                  "www"
#define MQTT_HOST_URI        "tcp://q6168186.ala.eu-central-1.emqxsl.com"
#define MQTT_SNI_HOST        "q6168186.ala.eu-central-1.emqxsl.com"
#define MQTT_PORT            8883
#define MQTT_USERNAME        "powerlytics-be"   // if any
#define MQTT_PASSWORD        "Power@123"   // if any
#define MQTT_TIMEOUT_MS      10000U

// Keepalive/publish tick
#define MQTT_KEEPALIVE_INTERVAL_MS   250000U
#define MQTT_RECONNECT_ATTEMPTS      3
#define MQTT_RECONNECT_DELAY_MS      5000U

// UART RX scratch buffer size for AT responses
#define MODEM_RX_BUF_SIZE    512



#endif


