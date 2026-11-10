#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include "esp_ota_ops.h"
#include <stdint.h>
#include <stdbool.h>

// ── Firmware version ──────────────────────────────────────────
#define FIRMWARE_VERSION        "1.0.0"
#define FIRMWARE_VERSION_NUM    100

// ── OTA server ────────────────────────────────────────────────
// Replace with your actual GitHub username and repo
#define OTA_VERSION_URL  "https://github.com/yourusername/" \
                         "ah-watch/releases/latest/download/" \
                         "version.txt"

#define OTA_FIRMWARE_URL "https://github.com/yourusername/" \
                         "ah-watch/releases/latest/download/" \
                         "ah_watch.bin"

// ── WiFi credentials ──────────────────────────────────────────
// In production store these in NVS not hardcoded
#define OTA_WIFI_SSID       "YourWiFiSSID"
#define OTA_WIFI_PASSWORD   "YourWiFiPassword"

// ── OTA timeouts ──────────────────────────────────────────────
#define OTA_WIFI_CONNECT_TIMEOUT_MS   10000
#define OTA_DOWNLOAD_TIMEOUT_MS       60000

// ── OTA state ─────────────────────────────────────────────────
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_CHECKING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_APPLYING,
    OTA_STATE_SUCCESS,
    OTA_STATE_FAILED,
    OTA_STATE_UP_TO_DATE
} ota_state_t;

// ── Progress callback ─────────────────────────────────────────
typedef void (*ota_progress_cb_t)(ota_state_t state,
                                   uint8_t     percent);

// ── Function declarations ─────────────────────────────────────
esp_err_t  ota_manager_init(void);
esp_err_t  ota_manager_check_and_update(void);
ota_state_t ota_manager_get_state(void);
void       ota_manager_register_progress_cb(ota_progress_cb_t cb);
esp_err_t  ota_manager_connect_wifi(void);
void       ota_manager_disconnect_wifi(void);
bool       ota_manager_is_wifi_connected(void);
const char *ota_manager_get_version(void);

#endif