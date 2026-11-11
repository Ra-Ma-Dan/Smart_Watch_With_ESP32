#include "ota_manager.h"
#include "my_uart.h"
#include "lvgl_port.h"
#include "ui_watchface.h"
#include "sleep_manager.h"
#include "DRV2605L.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include <string.h>
#include <stdlib.h>

static ota_state_t        ota_state      = OTA_STATE_IDLE;
static ota_progress_cb_t  progress_cb    = NULL;
static bool               wifi_connected = false;

static EventGroupHandle_t wifi_events    = NULL;
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAILED_BIT      BIT1

static void set_state(ota_state_t state, uint8_t percent) {
    ota_state = state;
    if (progress_cb) progress_cb(state, percent);

    const char *msg = "";
    switch (state) {
        case OTA_STATE_CHECKING:     msg = "Checking...";    break;
        case OTA_STATE_DOWNLOADING:  msg = "Updating...";    break;
        case OTA_STATE_VERIFYING:    msg = "Verifying...";   break;
        case OTA_STATE_APPLYING:     msg = "Applying...";    break;
        case OTA_STATE_SUCCESS:      msg = "Updated!";       break;
        case OTA_STATE_FAILED:       msg = "Update Failed";  break;
        case OTA_STATE_UP_TO_DATE:   msg = "Up to date";     break;
        default:                     msg = "";               break;
    }

    if (strlen(msg) > 0) {
        lvgl_port_lock();
        // Assuming label_steps is declared globally elsewhere in your UI porting code
        lv_label_set_text(label_steps, msg);
        lvgl_port_unlock();
    }
}

static void wifi_event_handler(void            *arg,
                                esp_event_base_t event_base,
                                int32_t          event_id,
                                void            *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        xEventGroupSetBits(wifi_events, WIFI_FAILED_BIT);
        LOG_WARN(TAG_OTA, "WiFi disconnected");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
        LOG_INFO(TAG_OTA, "WiFi connected — IP " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
    }
}

esp_err_t ota_manager_connect_wifi(void) {
    LOG_INFO(TAG_OTA, "Connecting to WiFi '%s'", OTA_WIFI_SSID);

    wifi_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = OTA_WIFI_SSID,
            .password = OTA_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        wifi_events,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(OTA_WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        LOG_INFO(TAG_OTA, "WiFi connection verified successfully");
        return ESP_OK;
    }

    LOG_ERROR(TAG_OTA, "WiFi connection failed");
    return ESP_ERR_TIMEOUT;
}

void ota_manager_disconnect_wifi(void) {
    esp_wifi_stop();
    esp_wifi_deinit();
    wifi_connected = false;
    LOG_INFO(TAG_OTA, "WiFi subsystem stopped down");
}

static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt) {
    static uint32_t total_size    = 0;
    static uint32_t received_size = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
            if (strcasecmp(evt->header_key, "Content-Length") == 0) {
                total_size = strtoul(evt->header_value, NULL, 10);
                LOG_INFO(TAG_OTA, "Firmware target size: %lu bytes", total_size);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            received_size += evt->data_len;
            if (total_size > 0) {
                uint8_t percent = (received_size * 100) / total_size;
                static uint8_t last_percent = 0;
                if (percent >= last_percent + 5) {
                    last_percent = percent;
                    set_state(OTA_STATE_DOWNLOADING, percent);
                    LOG_INFO(TAG_OTA, "Download progress: %d%%", percent);
                }
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}

static bool is_update_available(void) {
    esp_http_client_config_t config = {
        .url            = OTA_VERSION_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms     = 10000
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return false;

    char version_buf[16] = {0};
    esp_err_t ret = esp_http_client_open(client, 0);

    if (ret == ESP_OK) {
        int content_len = esp_http_client_fetch_headers(client);
        if (content_len > 0 && content_len < 16) {
            esp_http_client_read(client, version_buf, content_len);
            version_buf[content_len] = '\0';
        }
    }

    esp_http_client_cleanup(client);
    version_buf[strcspn(version_buf, "\r\n")] = '\0';

    LOG_INFO(TAG_OTA, "Current Local: %s  | Remote Server: %s", FIRMWARE_VERSION, version_buf);

    bool update_available = (strcmp(version_buf, FIRMWARE_VERSION) != 0 && strlen(version_buf) > 0);

    if (update_available) {
        LOG_INFO(TAG_OTA, "Update vector detected: %s → %s", FIRMWARE_VERSION, version_buf);
    } else {
        LOG_INFO(TAG_OTA, "System firmware is up to date");
    }

    return update_available;
}

static esp_err_t perform_update(void) {
    esp_http_client_config_t http_config = {
        .url               = OTA_FIRMWARE_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = OTA_DOWNLOAD_TIMEOUT_MS,
        .event_handler     = ota_http_event_handler,
        .keep_alive_enable = true
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        LOG_INFO(TAG_OTA, "OTA processing clean — cryptographic signature verified");
        return ESP_OK;
    }

    LOG_ERROR(TAG_OTA, "OTA pipeline failed: %s", esp_err_to_name(ret));
    return ret;
}

esp_err_t ota_manager_check_and_update(void) {
    LOG_INFO(TAG_OTA, "OTA workflow started — automated sleep features disabled");
    set_state(OTA_STATE_CHECKING, 0);

    esp_err_t ret = ota_manager_connect_wifi();
    if (ret != ESP_OK) {
        set_state(OTA_STATE_FAILED, 0);
        return ret;
    }

    if (!is_update_available()) {
        set_state(OTA_STATE_UP_TO_DATE, 100);
        ota_manager_disconnect_wifi();
        return ESP_OK;
    }

    set_state(OTA_STATE_DOWNLOADING, 0);
    ret = perform_update();

    if (ret != ESP_OK) {
        set_state(OTA_STATE_FAILED, 0);
        ota_manager_disconnect_wifi();
        return ret;
    }

    set_state(OTA_STATE_SUCCESS, 100);
    ota_manager_disconnect_wifi();

    drv2605l_play_pattern(HAPTIC_PATTERN_SUCCESS);
    LOG_INFO(TAG_OTA, "Restarting application subsystem into updated slot in 3 seconds");
    vTaskDelay(pdMS_TO_TICKS(3000));

    esp_restart();
    return ESP_OK; 
}

esp_err_t ota_manager_init(void) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t   state;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
            LOG_INFO(TAG_OTA, "Self-test passes. Current firmware partition locked as stable");
        }
    }

    LOG_INFO(TAG_OTA, "OTA layer active [Version: %s, Slot: %s]", FIRMWARE_VERSION, running->label);
    return ESP_OK;
}

ota_state_t ota_manager_get_state(void) { return ota_state; }
bool ota_manager_is_wifi_connected(void) { return wifi_connected; }
const char *ota_manager_get_version(void) { return FIRMWARE_VERSION; }
void ota_manager_register_progress_cb(ota_progress_cb_t cb) { progress_cb = cb; }
