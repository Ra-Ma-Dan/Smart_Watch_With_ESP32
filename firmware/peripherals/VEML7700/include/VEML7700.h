#ifndef VEML7700_H
#define VEML7700_H

#include "i2c_driver.h"
#include "uart_driver.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define VEML7700_ADDR           I2C_ADDR_VEML7700   // 0x10

// ── Registers ─────────────────────────────────────────────────
#define VEML_REG_CONFIG         0x00
#define VEML_REG_THRESH_HIGH    0x01
#define VEML_REG_THRESH_LOW     0x02
#define VEML_REG_POWER_SAVE     0x03
#define VEML_REG_ALS            0x04
#define VEML_REG_WHITE          0x05
#define VEML_REG_INT_STATUS     0x06

// ── Gain settings ─────────────────────────────────────────────
#define VEML_GAIN_1             0x0000
#define VEML_GAIN_2             0x0800
#define VEML_GAIN_1_8           0x1000
#define VEML_GAIN_1_4           0x1800

// ── Integration time ──────────────────────────────────────────
#define VEML_IT_25MS            0x03 << 6
#define VEML_IT_50MS            0x02 << 6
#define VEML_IT_100MS           0x00 << 6
#define VEML_IT_200MS           0x01 << 6
#define VEML_IT_400MS           0x02 << 2
#define VEML_IT_800MS           0x03 << 2

// ── Power ─────────────────────────────────────────────────────
#define VEML_POWER_ON           0x0000
#define VEML_POWER_OFF          0x0001

// ── Display brightness mapping ────────────────────────────────
#define VEML_LUX_DARK           5.0f      // below this = min brightness
#define VEML_LUX_DIM            50.0f     // indoor low light
#define VEML_LUX_INDOOR         500.0f    // normal indoor
#define VEML_LUX_OUTDOOR        5000.0f   // overcast outdoor
#define VEML_LUX_BRIGHT         50000.0f  // direct sunlight

// ── Data struct ───────────────────────────────────────────────
typedef struct {
    float    lux;              // calculated lux value
    uint16_t raw_als;          // raw ALS count
    uint16_t raw_white;        // raw white channel count
    uint8_t  brightness;       // 0-255 suggested display brightness
    bool     saturated;        // true if sensor is saturated
} veml7700_data_t;

// ── Function declarations ─────────────────────────────────────
esp_err_t  veml7700_init(void);
esp_err_t  veml7700_read(veml7700_data_t *data);
float      veml7700_get_lux(void);
uint8_t    veml7700_get_brightness(void);
esp_err_t  veml7700_shutdown(void);
esp_err_t  veml7700_wakeup(void);

#endif