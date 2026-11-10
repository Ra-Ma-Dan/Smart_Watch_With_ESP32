#ifndef BLE_DRIVER_H
#define BLE_DRIVER_H

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include <stdint.h>
#include <stdbool.h>

// ── Device name ───────────────────────────────────────────────
#define BLE_DEVICE_NAME         "AH-Watch"

// ── Connection state ──────────────────────────────────────────
typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
    BLE_STATE_DISCONNECTED
} ble_state_t;

// ── Notification data structs ─────────────────────────────────

typedef struct {
    uint8_t  heart_rate;
    uint8_t  spo2;
    uint32_t steps;
    float    temperature;
    uint8_t  battery;
} ble_health_data_t;

typedef struct {
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
} ble_time_data_t;

typedef struct {
    uint8_t  type;         // 0=SMS 1=call 2=email 3=app
    char     title[32];
    char     body[64];
} ble_notification_t;

// ── Callbacks ─────────────────────────────────────────────────
typedef void (*ble_connected_cb_t)(void);
typedef void (*ble_disconnected_cb_t)(void);
typedef void (*ble_time_sync_cb_t)(ble_time_data_t time);
typedef void (*ble_notification_cb_t)(ble_notification_t notif);

// ── Function declarations ─────────────────────────────────────
esp_err_t  ble_driver_init(void);
esp_err_t  ble_driver_start_advertising(void);
esp_err_t  ble_driver_stop_advertising(void);
ble_state_t ble_driver_get_state(void);
bool       ble_driver_is_connected(void);

// Send health data to phone
esp_err_t  ble_driver_notify_heart_rate(uint8_t bpm);
esp_err_t  ble_driver_notify_spo2(uint8_t spo2);
esp_err_t  ble_driver_notify_steps(uint32_t steps);
esp_err_t  ble_driver_notify_temperature(float celsius);
esp_err_t  ble_driver_notify_battery(uint8_t percent);
esp_err_t  ble_driver_notify_sos(void);

// Register callbacks
void ble_driver_register_connected_cb(ble_connected_cb_t cb);
void ble_driver_register_disconnected_cb(ble_disconnected_cb_t cb);
void ble_driver_register_time_sync_cb(ble_time_sync_cb_t cb);
void ble_driver_register_notification_cb(ble_notification_cb_t cb);

#endif