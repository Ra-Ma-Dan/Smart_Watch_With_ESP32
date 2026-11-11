#ifndef TMP117_H
#define TMP117_H

#include "my_i2c.h"
#include "my_uart.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define TMP117_ADDR            I2C_ADDR_TMP117   // 0x48

// ── Registers ─────────────────────────────────────────────────
#define TMP_REG_RESULT         0x00
#define TMP_REG_CONFIG         0x01
#define TMP_REG_HIGH_LIMIT     0x02
#define TMP_REG_LOW_LIMIT      0x03
#define TMP_REG_DEVICE_ID      0x0F

// ── Device ID ─────────────────────────────────────────────────
#define TMP117_DEVICE_ID       0x0117

// ── Configuration values ──────────────────────────────────────
#define TMP_MODE_CONTINUOUS    0x0000
#define TMP_MODE_SHUTDOWN      0x0400
#define TMP_MODE_ONESHOT       0x0C00

#define TMP_AVG_NONE           0x0000
#define TMP_AVG_8              0x0020
#define TMP_AVG_32             0x0040
#define TMP_AVG_64             0x0060

#define TMP_CYCLE_15MS         0x0000
#define TMP_CYCLE_125MS        0x0080
#define TMP_CYCLE_250MS        0x0100
#define TMP_CYCLE_500MS        0x0180
#define TMP_CYCLE_1S           0x0200
#define TMP_CYCLE_4S           0x0280
#define TMP_CYCLE_8S           0x0300
#define TMP_CYCLE_16S          0x0380

// ── Temperature resolution ────────────────────────────────────
#define TMP117_RESOLUTION      0.0078125f

// ── Alert thresholds for skin temp ────────────────────────────
#define TMP_ALERT_HIGH_C       38.5f    // possible fever
#define TMP_ALERT_LOW_C        34.0f    // too cold, poor contact

// ── Data struct ───────────────────────────────────────────────
typedef struct {
    float    celsius;
    float    fahrenheit;
    bool     data_ready;
    bool     high_alert;
    bool     low_alert;
} tmp117_data_t;

// ── Function declarations ─────────────────────────────────────
esp_err_t  tmp117_init(void);
esp_err_t  tmp117_read(tmp117_data_t *data);
float      tmp117_get_celsius(void);
float      tmp117_get_fahrenheit(void);
esp_err_t  tmp117_trigger_oneshot(void);
bool       tmp117_data_ready(void);
esp_err_t  tmp117_set_alerts(float high_c, float low_c);
esp_err_t  tmp117_shutdown(void);

#endif