#ifndef BMI270_H
#define BMI270_H

#include "i2c_driver.h"
#include "uart_driver.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define BMI270_ADDR             I2C_ADDR_BMI270   // 0x69

// ── Chip ID ───────────────────────────────────────────────────
#define BMI270_CHIP_ID          0x24

// ── Registers ─────────────────────────────────────────────────
#define BMI_REG_CHIP_ID         0x00
#define BMI_REG_ERROR           0x02
#define BMI_REG_STATUS          0x03
#define BMI_REG_ACC_X_LSB       0x04
#define BMI_REG_GYR_X_LSB       0x0C
#define BMI_REG_INT_STATUS0     0x1C
#define BMI_REG_INT_STATUS1     0x1D
#define BMI_REG_STEP_CNT_L      0x1E   // step count low byte
#define BMI_REG_STEP_CNT_H      0x1F   // step count high byte
#define BMI_REG_INTERNAL_STAT   0x21
#define BMI_REG_ACC_CONF        0x40
#define BMI_REG_ACC_RANGE       0x41
#define BMI_REG_GYR_CONF        0x42
#define BMI_REG_GYR_RANGE       0x43
#define BMI_REG_INT1_IO_CTRL    0x53
#define BMI_REG_INT_MAP_DATA    0x58
#define BMI_REG_INIT_CTRL       0x59
#define BMI_REG_INIT_ADDR_0     0x5B
#define BMI_REG_INIT_ADDR_1     0x5C
#define BMI_REG_INIT_DATA       0x5E
#define BMI_REG_PWR_CONF        0x7C
#define BMI_REG_PWR_CTRL        0x7D
#define BMI_REG_CMD             0x7E

// ── Commands ──────────────────────────────────────────────────
#define BMI_CMD_SOFT_RESET      0xB6
#define BMI_CMD_FIFO_FLUSH      0xB0

// ── Accel range ───────────────────────────────────────────────
#define BMI_ACC_RANGE_2G        0x00   // ±2g,  sensitivity 16384 LSB/g
#define BMI_ACC_RANGE_4G        0x01   // ±4g,  sensitivity 8192  LSB/g
#define BMI_ACC_RANGE_8G        0x02   // ±8g,  sensitivity 4096  LSB/g
#define BMI_ACC_RANGE_16G       0x03   // ±16g, sensitivity 2048  LSB/g

// ── Gyro range ────────────────────────────────────────────────
#define BMI_GYR_RANGE_2000      0x00   // ±2000 dps, 16.4 LSB/dps
#define BMI_GYR_RANGE_1000      0x01   // ±1000 dps, 32.8 LSB/dps
#define BMI_GYR_RANGE_500       0x02   // ±500  dps, 65.6 LSB/dps
#define BMI_GYR_RANGE_250       0x03   // ±250  dps, 131.2 LSB/dps
#define BMI_GYR_RANGE_125       0x04   // ±125  dps, 262.4 LSB/dps

// ── Output data rates ─────────────────────────────────────────
#define BMI_ACC_ODR_25HZ        0x06
#define BMI_ACC_ODR_50HZ        0x07
#define BMI_ACC_ODR_100HZ       0x08
#define BMI_ACC_ODR_200HZ       0x09
#define BMI_GYR_ODR_25HZ        0x06
#define BMI_GYR_ODR_100HZ       0x08
#define BMI_GYR_ODR_200HZ       0x09

// ── Sensitivity constants ─────────────────────────────────────
#define BMI_ACC_SENSITIVITY_2G  16384.0f
#define BMI_GYR_SENSITIVITY_2000 16.4f

// ── Interrupt status bits ─────────────────────────────────────
#define BMI_INT_STEP_COUNTER    (1 << 0)
#define BMI_INT_FALL_DETECTED   (1 << 2)
#define BMI_INT_WRIST_GESTURE   (1 << 3)
#define BMI_INT_WRIST_WAKEUP    (1 << 4)

// ── Data structs ──────────────────────────────────────────────
typedef struct {
    float x;   // g-force
    float y;
    float z;
} bmi270_accel_t;

typedef struct {
    float x;   // degrees per second
    float y;
    float z;
} bmi270_gyro_t;

typedef struct {
    bmi270_accel_t accel;
    bmi270_gyro_t  gyro;
    uint32_t       steps;
    bool           fall_detected;
    bool           wrist_raised;
} bmi270_data_t;

// ── Function declarations ─────────────────────────────────────
esp_err_t  bmi270_init(void);
esp_err_t  bmi270_read_accel(bmi270_accel_t *accel);
esp_err_t  bmi270_read_gyro(bmi270_gyro_t *gyro);
esp_err_t  bmi270_read_all(bmi270_data_t *data);
uint32_t   bmi270_get_step_count(void);
esp_err_t  bmi270_reset_steps(void);
bool       bmi270_fall_detected(void);
bool       bmi270_wrist_raised(void);
esp_err_t  bmi270_handle_interrupt(bmi270_data_t *data);
esp_err_t  bmi270_set_power_mode(bool low_power);

#endif