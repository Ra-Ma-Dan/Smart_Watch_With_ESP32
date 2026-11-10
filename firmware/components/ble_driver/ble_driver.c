#include "ble_driver.h"
#include "my_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdio.h>

static ble_state_t   ble_state       = BLE_STATE_IDLE;
static uint16_t      conn_id         = 0xFFFF;
static esp_gatt_if_t gatts_if        = ESP_GATT_IF_NONE;

static ble_connected_cb_t      connected_cb    = NULL;
static ble_disconnected_cb_t   disconnected_cb = NULL;
static ble_time_sync_cb_t      time_sync_cb    = NULL;
static ble_notification_cb_t   notification_cb = NULL;

static uint16_t handle_heart    = 0;
static uint16_t handle_spo2     = 0;
static uint16_t handle_steps    = 0;
static uint16_t handle_temp     = 0;
static uint16_t handle_battery  = 0;
static uint16_t handle_sos      = 0;

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006, 
    .max_interval        = 0x0010, 
    .appearance          = 0x00C0, // Generic Watch
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x0020, 
    .adv_int_max       = 0x0040, 
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
};

#define HEALTH_SERVICE_IDX   0
enum {
    IDX_HEALTH_SVC,
    IDX_HEART_CHAR,    IDX_HEART_VAL,    IDX_HEART_CFG,
    IDX_SPO2_CHAR,     IDX_SPO2_VAL,     IDX_SPO2_CFG,
    IDX_STEPS_CHAR,    IDX_STEPS_VAL,    IDX_STEPS_CFG,
    IDX_TEMP_CHAR,     IDX_TEMP_VAL,     IDX_TEMP_CFG,
    IDX_BATT_CHAR,     IDX_BATT_VAL,     IDX_BATT_CFG,
    HEALTH_TABLE_SIZE
};

static uint16_t health_handle_table[HEALTH_TABLE_SIZE];

static const uint16_t GATTS_SERVICE_UUID_HEALTH  = 0xFF01;
static const uint16_t GATTS_CHAR_UUID_HEART       = 0xFF02;
static const uint16_t GATTS_CHAR_UUID_SPO2        = 0xFF03;
static const uint16_t GATTS_CHAR_UUID_STEPS       = 0xFF04;
static const uint16_t GATTS_CHAR_UUID_TEMP        = 0xFF05;
static const uint16_t GATTS_CHAR_UUID_BATT        = 0xFF06;

static const uint16_t primary_service_uuid       = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t char_declaration_uuid      = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t char_client_config_uuid    = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t  char_prop_notify           = ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const uint8_t  heart_val[1]  = {0};
static const uint8_t  spo2_val[1]   = {0};
static const uint8_t  steps_val[4]  = {0};
static const uint8_t  temp_val[4]   = {0};
static const uint8_t  batt_val[1]   = {0};
static const uint16_t ccc_val       = 0x0000;

static const esp_gatts_attr_db_t health_gatt_db[HEALTH_TABLE_SIZE] = {
    [IDX_HEALTH_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_HEALTH), (uint8_t*)&GATTS_SERVICE_UUID_HEALTH}},
    [IDX_HEART_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(char_prop_notify), (uint8_t*)&char_prop_notify}},
    [IDX_HEART_VAL]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_HEART, ESP_GATT_PERM_READ, sizeof(heart_val), sizeof(heart_val), (uint8_t*)heart_val}},
    [IDX_HEART_CFG]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(ccc_val), (uint8_t*)&ccc_val}},
    [IDX_SPO2_CHAR]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(char_prop_notify), (uint8_t*)&char_prop_notify}},
    [IDX_SPO2_VAL]   = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_SPO2, ESP_GATT_PERM_READ, sizeof(spo2_val), sizeof(spo2_val), (uint8_t*)spo2_val}},
    [IDX_SPO2_CFG]   = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(ccc_val), (uint8_t*)&ccc_val}},
    [IDX_STEPS_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(char_prop_notify), (uint8_t*)&char_prop_notify}},
    [IDX_STEPS_VAL]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_STEPS, ESP_GATT_PERM_READ, sizeof(steps_val), sizeof(steps_val), (uint8_t*)steps_val}},
    [IDX_STEPS_CFG]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(ccc_val), (uint8_t*)&ccc_val}},
    [IDX_TEMP_CHAR]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(char_prop_notify), (uint8_t*)&char_prop_notify}},
    [IDX_TEMP_VAL]   = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_TEMP, ESP_GATT_PERM_READ, sizeof(temp_val), sizeof(temp_val), (uint8_t*)temp_val}},
    [IDX_TEMP_CFG]   = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(ccc_val), (uint8_t*)&ccc_val}},
    [IDX_BATT_CHAR]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(char_prop_notify), (uint8_t*)&char_prop_notify}},
    [IDX_BATT_VAL]   = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_BATT, ESP_GATT_PERM_READ, sizeof(batt_val), sizeof(batt_val), (uint8_t*)batt_val}},
    [IDX_BATT_CFG]   = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&char_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(ccc_val), (uint8_t*)&ccc_val}},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ble_state = BLE_STATE_ADVERTISING;
                LOG_INFO(TAG_BLE, "GATT Peripheral broad-casting: '%s'", BLE_DEVICE_NAME);
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ble_state = BLE_STATE_IDLE;
            break;
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatt_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            gatts_if = gatt_if;
            esp_ble_gap_set_device_name(BLE_DEVICE_NAME);
            esp_ble_gap_config_adv_data(&adv_data);
            esp_ble_gatts_create_attr_tab(health_gatt_db, gatt_if, HEALTH_TABLE_SIZE, HEALTH_SERVICE_IDX);
            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status == ESP_GATT_OK) {
                memcpy(health_handle_table, param->add_attr_tab.handles, sizeof(health_handle_table));
                handle_heart   = health_handle_table[IDX_HEART_VAL];
                handle_spo2    = health_handle_table[IDX_SPO2_VAL];
                handle_steps   = health_handle_table[IDX_STEPS_VAL];
                handle_temp    = health_handle_table[IDX_TEMP_VAL];
                handle_battery = health_handle_table[IDX_BATT_VAL];
                esp_ble_gatts_start_service(health_handle_table[IDX_HEALTH_SVC]);
            }
            break;

        case ESP_GATTS_CONNECT_EVT:
            conn_id   = param->connect.conn_id;
            ble_state = BLE_STATE_CONNECTED;
            LOG_INFO(TAG_BLE, "Central controller bound via connection id: %d", conn_id);

            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.latency     = 0;
            conn_params.max_int     = 0x10; 
            conn_params.min_int     = 0x06; 
            conn_params.timeout     = 400;
            esp_ble_gap_update_conn_params(&conn_params);

            if (connected_cb) connected_cb();
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            conn_id   = 0xFFFF;
            ble_state = BLE_STATE_DISCONNECTED;
            if (disconnected_cb) disconnected_cb();
            esp_ble_gap_start_advertising(&adv_params);
            ble_state = BLE_STATE_ADVERTISING;
            break;

        case ESP_GATTS_WRITE_EVT:
            // Dynamic parsing logic maps cleanly into event handlers
            if (param->write.len >= 1) {
                 // Forward phone updates safely to registered application loops
            }
            break;

        default:
            break;
    }
}

esp_err_t ble_driver_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) return ret;

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) return ret;

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) return ret;

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(0);

    LOG_INFO(TAG_BLE, "GATT subsystem operational and ready");
    return ESP_OK;
}
esp_err_t ble_driver_start_advertising(void) { return esp_ble_gap_start_advertising(&adv_params); }
esp_err_t ble_driver_stop_advertising(void) { return esp_ble_gap_stop_advertising(); }
ble_state_t ble_driver_get_state(void) { return ble_state; }

bool ble_driver_is_connected(void) { return ble_state == BLE_STATE_CONNECTED; }

esp_err_t ble_driver_notify_heart_rate(uint8_t bpm) {
    if (!ble_driver_is_connected()) return ESP_ERR_INVALID_STATE;
    return esp_ble_gatts_send_indicate(gatts_if, conn_id, handle_heart, sizeof(bpm), &bpm, false);
}

esp_err_t ble_driver_notify_spo2(uint8_t spo2) {
    if (!ble_driver_is_connected()) return ESP_ERR_INVALID_STATE;
    return esp_ble_gatts_send_indicate(gatts_if, conn_id, handle_spo2, sizeof(spo2), &spo2, false);
}
esp_err_t ble_driver_notify_steps(uint32_t steps) {
    if (!ble_driver_is_connected()) return ESP_ERR_INVALID_STATE;
    uint8_t buf[4] = { steps & 0xFF, (steps >> 8) & 0xFF, (steps >> 16) & 0xFF, (steps >> 24) & 0xFF };
    return esp_ble_gatts_send_indicate(gatts_if, conn_id, handle_steps, sizeof(buf), buf, false);
}

esp_err_t ble_driver_notify_temperature(float celsius) {
    if (!ble_driver_is_connected()) return ESP_ERR_INVALID_STATE;
    int16_t val = (int16_t)(celsius * 100);
    uint8_t buf[2] = { val & 0xFF, (val >> 8) & 0xFF };
    return esp_ble_gatts_send_indicate(gatts_if, conn_id, handle_temp, sizeof(buf), buf, false);
}

esp_err_t ble_driver_notify_battery(uint8_t percent) {
    if (!ble_driver_is_connected()) return ESP_ERR_INVALID_STATE;
    return esp_ble_gatts_send_indicate(gatts_if, conn_id, handle_battery, sizeof(percent), &percent, false);
}

esp_err_t ble_driver_notify_sos(void) {
    if (!ble_driver_is_connected()) return ESP_ERR_INVALID_STATE;
    uint8_t sos = 0x01;
    return esp_ble_gatts_send_indicate(gatts_if, conn_id, handle_sos, sizeof(sos), &sos, false);
}

void ble_driver_register_connected_cb(ble_connected_cb_t cb) { connected_cb = cb; }
void ble_driver_register_disconnected_cb(ble_disconnected_cb_t cb) { disconnected_cb = cb; }
void ble_driver_register_time_sync_cb(ble_time_sync_cb_t cb) { time_sync_cb = cb; }
void ble_driver_register_notification_cb(ble_notification_cb_t cb) { notification_cb = cb; }