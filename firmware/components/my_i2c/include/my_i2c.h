#ifndef MY_I2C_H
#define MY_I2C_H

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

// ── Bus hardware configuration ────────────────────────────────
#define I2C_PORT        I2C_NUM_0
#define I2C_SDA_PIN     GPIO_NUM_8
#define I2C_SCL_PIN     GPIO_NUM_9
#define I2C_CLOCK_HZ    400000       // 400 kHz fast mode
#define I2C_TIMEOUT_MS  100

// ── Explicit device handle declarations ───────────────────────
// Downstream drivers can extern these handles to cleanly make calls
extern i2c_master_dev_handle_t veml7700_handle;
extern i2c_master_dev_handle_t cst816s_handle;
extern i2c_master_dev_handle_t axp2101_handle;
extern i2c_master_dev_handle_t max17048_handle;
extern i2c_master_dev_handle_t tmp117_handle;
extern i2c_master_dev_handle_t pcf85063a_handle;
extern i2c_master_dev_handle_t max30102_handle;
extern i2c_master_dev_handle_t drv2605l_handle;
extern i2c_master_dev_handle_t bmi270_handle;

// ── Modernized Function declarations ──────────────────────────
// Notice we accept the explicit 'dev_handle' instead of raw 'dev_addr'
esp_err_t i2c_driver_init(void);

esp_err_t i2c_driver_write(i2c_master_dev_handle_t dev_handle,
                            uint8_t                  reg_addr,
                            const uint8_t           *data,
                            size_t                   len);

esp_err_t i2c_driver_read(i2c_master_dev_handle_t dev_handle,
                           uint8_t                  reg_addr,
                           uint8_t                 *buffer,
                           size_t                   len);

esp_err_t i2c_driver_write_byte(i2c_master_dev_handle_t dev_handle,
                                 uint8_t                  reg_addr,
                                 uint8_t                  value);

esp_err_t i2c_driver_read_byte(i2c_master_dev_handle_t dev_handle,
                                uint8_t                  reg_addr,
                                uint8_t                 *value);

esp_err_t i2c_driver_scan(void);

#endif // MY_I2C_H
