#ifndef MAX17048_H
#define MAX17048_H

#include "my_i2c.h"
#include "my_uart.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define MAX17048_ADDR         I2C_ADDR_MAX17048   // 0x36

// ── Registers ─────────────────────────────────────────────────
#define MAX_REG_VCELL         0x02
#define MAX_REG_SOC           0x04
#define MAX_REG_MODE          0x06
#define MAX_REG_VERSION       0x08
#define MAX_REG_HIBRT         0x0A
#define MAX_REG_CONFIG        0x0C
#define MAX_REG_CRATE         0x14
#define MAX_REG_COMMAND       0xFE

// ── Alert threshold ───────────────────────────────────────────
// Fire interrupt when battery drops below this percentage
#define MAX_ALERT_THRESHOLD   15    // alert at 15%

// ── Battery data struct ───────────────────────────────────────
typedef struct {
    float    voltage_mv;     // battery voltage in millivolts
    float    percentage;     // 0.0 to 100.0
    float    charge_rate;    // %/hr, negative = discharging
    bool     is_charging;    // true if charge rate is positive
    bool     alert;          // true if below alert threshold
} max17048_data_t;

// ── Function declarations ─────────────────────────────────────
esp_err_t  max17048_init(void);
esp_err_t  max17048_read(max17048_data_t *data);
float      max17048_get_voltage(void);
float      max17048_get_percentage(void);
float      max17048_get_charge_rate(void);
esp_err_t  max17048_set_alert_threshold(uint8_t percent);
esp_err_t  max17048_clear_alert(void);
esp_err_t  max17048_hibernate(bool enable);
esp_err_t  max17048_quick_start(void);

#endif