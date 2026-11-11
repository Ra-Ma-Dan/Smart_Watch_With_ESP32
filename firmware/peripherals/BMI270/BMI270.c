#include "BMI270.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// ── BMI270 config file ────────────────────────────────────────
// Full 8192 byte array from Bosch GitHub:
// github.com/boschsensortec/BMI270_SensorAPI
// Shortened here for clarity — use the real file in production

extern const uint8_t bmi270_config_file[];
extern const uint16_t bmi270_config_file_size;

// ── Current sensitivity based on range setting ────────────────
static float acc_sensitivity = BMI_ACC_SENSITIVITY_2G;
static float gyr_sensitivity = BMI_GYR_SENSITIVITY_2000;

// ── Write config file to chip ─────────────────────────────────

static esp_err_t load_config_file(void) {

    // Disable advanced power save before config load
    i2c_driver_write_byte(BMI270_ADDR, BMI_REG_PWR_CONF, 0x00);
    vTaskDelay(pdMS_TO_TICKS(1));

    // Prepare chip for config load
    i2c_driver_write_byte(BMI270_ADDR, BMI_REG_INIT_CTRL, 0x00);

    // Write config file in 64 byte chunks
    uint16_t index = 0;
    while (index < bmi270_config_file_size) {

        // Set write address in chip
        uint8_t addr_lo = (index / 2) & 0x0F;
        uint8_t addr_hi = (index / 2) >> 4;
        i2c_driver_write_byte(BMI270_ADDR,
                               BMI_REG_INIT_ADDR_0, addr_lo);
        i2c_driver_write_byte(BMI270_ADDR,
                               BMI_REG_INIT_ADDR_1, addr_hi);

        // Write 64 bytes chunk
        uint16_t chunk = 64;
        if (index + chunk > bmi270_config_file_size) {
            chunk = bmi270_config_file_size - index;
        }

        i2c_driver_write(BMI270_ADDR,
                          BMI_REG_INIT_DATA,
                          (uint8_t *)&bmi270_config_file[index],
                          chunk);
        index += chunk;
    }

    // Signal config load complete
    i2c_driver_write_byte(BMI270_ADDR, BMI_REG_INIT_CTRL, 0x01);
    vTaskDelay(pdMS_TO_TICKS(150));

    // Check internal status — should be 0x01 if loaded correctly
    uint8_t status = 0;
    i2c_driver_read_byte(BMI270_ADDR,
                          BMI_REG_INTERNAL_STAT,
                          &status);

    if ((status & 0x0F) != 0x01) {
        LOG_ERROR(TAG_IMU,
                  "Config file load failed — status 0x%02X",
                  status);
        return ESP_FAIL;
    }

    LOG_INFO(TAG_IMU, "Config file loaded successfully");
    return ESP_OK;
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t bmi270_init(void) {

    // Confirm chip ID
    uint8_t id = 0;
    esp_err_t ret = i2c_driver_read_byte(BMI270_ADDR,
                                          BMI_REG_CHIP_ID,
                                          &id);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_IMU, "BMI270 not found on I2C bus");
        return ret;
    }

    if (id != BMI270_CHIP_ID) {
        LOG_ERROR(TAG_IMU,
                  "Wrong chip ID 0x%02X expected 0x%02X",
                  id, BMI270_CHIP_ID);
        return ESP_ERR_NOT_FOUND;
    }

    LOG_INFO(TAG_IMU, "BMI270 found — chip ID 0x%02X", id);

    // Soft reset
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_CMD,
                           BMI_CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Load config file — required before any feature works
    ret = load_config_file();
    if (ret != ESP_OK) return ret;

    // ── Configure accelerometer ───────────────────────────────
    // ODR 100Hz, normal bandwidth
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_ACC_CONF,
                           BMI_ACC_ODR_100HZ | 0xA0);

    // ±2g range — best resolution for wrist movement
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_ACC_RANGE,
                           BMI_ACC_RANGE_2G);
    acc_sensitivity = BMI_ACC_SENSITIVITY_2G;

    // ── Configure gyroscope ───────────────────────────────────
    // ODR 200Hz, normal bandwidth
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_GYR_CONF,
                           BMI_GYR_ODR_200HZ | 0xA0);

    // ±2000 dps range for gesture detection
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_GYR_RANGE,
                           BMI_GYR_RANGE_2000);
    gyr_sensitivity = BMI_GYR_SENSITIVITY_2000;

    // ── Enable sensors ────────────────────────────────────────
    // Bit 2 = accel, bit 1 = gyro, bit 0 = temp
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_PWR_CTRL,
                           0x0E);
    vTaskDelay(pdMS_TO_TICKS(10));

    // ── Configure interrupt pin ───────────────────────────────
    // INT1 active high, push-pull output
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_INT1_IO_CTRL,
                           0x0A);

    // Map step counter and fall detection to INT1
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_INT_MAP_DATA,
                           0x04);

    // Enable advanced power save
    i2c_driver_write_byte(BMI270_ADDR,
                           BMI_REG_PWR_CONF,
                           0x03);

    LOG_INFO(TAG_IMU, "BMI270 initialized — accel + gyro + steps + fall");
    return ESP_OK;
}

// ── Read accelerometer ────────────────────────────────────────

esp_err_t bmi270_read_accel(bmi270_accel_t *accel) {
    uint8_t buf[6] = {0};
    esp_err_t ret  = i2c_driver_read(BMI270_ADDR,
                                      BMI_REG_ACC_X_LSB,
                                      buf, 6);
    if (ret != ESP_OK) return ret;

    // Each axis is 16 bit signed little endian
    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    accel->x = raw_x / acc_sensitivity;
    accel->y = raw_y / acc_sensitivity;
    accel->z = raw_z / acc_sensitivity;

    return ESP_OK;
}

// ── Read gyroscope ────────────────────────────────────────────

esp_err_t bmi270_read_gyro(bmi270_gyro_t *gyro) {
    uint8_t buf[6] = {0};
    esp_err_t ret  = i2c_driver_read(BMI270_ADDR,
                                      BMI_REG_GYR_X_LSB,
                                      buf, 6);
    if (ret != ESP_OK) return ret;

    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    gyro->x = raw_x / gyr_sensitivity;
    gyro->y = raw_y / gyr_sensitivity;
    gyro->z = raw_z / gyr_sensitivity;

    return ESP_OK;
}

// ── Read all data ─────────────────────────────────────────────

esp_err_t bmi270_read_all(bmi270_data_t *data) {
    esp_err_t ret;

    ret = bmi270_read_accel(&data->accel);
    if (ret != ESP_OK) return ret;

    ret = bmi270_read_gyro(&data->gyro);
    if (ret != ESP_OK) return ret;

    data->steps         = bmi270_get_step_count();
    data->fall_detected = bmi270_fall_detected();
    data->wrist_raised  = bmi270_wrist_raised();

    return ESP_OK;
}

// ── Step counter ──────────────────────────────────────────────

uint32_t bmi270_get_step_count(void) {
    uint8_t lo = 0, hi = 0;
    i2c_driver_read_byte(BMI270_ADDR, BMI_REG_STEP_CNT_L, &lo);
    i2c_driver_read_byte(BMI270_ADDR, BMI_REG_STEP_CNT_H, &hi);
    return ((uint32_t)hi << 8) | lo;
}

esp_err_t bmi270_reset_steps(void) {
    // Write reset command to CMD register
    LOG_INFO(TAG_IMU, "Step counter reset");
    return i2c_driver_write_byte(BMI270_ADDR,
                                  BMI_REG_CMD, 0x02);
}

// ── Fall detection ────────────────────────────────────────────

bool bmi270_fall_detected(void) {
    uint8_t status = 0;
    i2c_driver_read_byte(BMI270_ADDR,
                          BMI_REG_INT_STATUS0,
                          &status);
    return (status & BMI_INT_FALL_DETECTED) != 0;
}

// ── Wrist raise detection ─────────────────────────────────────

bool bmi270_wrist_raised(void) {
    uint8_t status = 0;
    i2c_driver_read_byte(BMI270_ADDR,
                          BMI_REG_INT_STATUS0,
                          &status);
    return (status & BMI_INT_WRIST_WAKEUP) != 0;
}

// ── Handle interrupt ──────────────────────────────────────────

esp_err_t bmi270_handle_interrupt(bmi270_data_t *data) {
    uint8_t status = 0;
    i2c_driver_read_byte(BMI270_ADDR,
                          BMI_REG_INT_STATUS0,
                          &status);

    data->steps         = bmi270_get_step_count();
    data->fall_detected = (status & BMI_INT_FALL_DETECTED) != 0;
    data->wrist_raised  = (status & BMI_INT_WRIST_WAKEUP)  != 0;

    if (data->fall_detected) {
        LOG_WARN(TAG_IMU, "FALL DETECTED");
    }

    if (data->wrist_raised) {
        LOG_INFO(TAG_IMU, "Wrist raised — wake display");
    }

    LOG_INFO(TAG_IMU, "Steps: %lu", data->steps);

    return ESP_OK;
}

// ── Power mode ────────────────────────────────────────────────

esp_err_t bmi270_set_power_mode(bool low_power) {
    if (low_power) {
        // Disable gyro, keep accel for step counting
        i2c_driver_write_byte(BMI270_ADDR,
                               BMI_REG_PWR_CTRL, 0x04);
        LOG_INFO(TAG_IMU, "Low power — gyro off, accel on");
    } else {
        // Enable both
        i2c_driver_write_byte(BMI270_ADDR,
                               BMI_REG_PWR_CTRL, 0x0E);
        LOG_INFO(TAG_IMU, "Full power — accel + gyro on");
    }
    return ESP_OK;
}