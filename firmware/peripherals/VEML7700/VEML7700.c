#include "veml7700.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

// ── Current gain and integration time ────────────────────────
static uint16_t current_gain = VEML_GAIN_1;
static uint8_t  current_it   = VEML_IT_100MS;

// ── Resolution lookup ─────────────────────────────────────────
// lux per count at each gain+integration time combination
// Based on VEML7700 application note

static float get_resolution(void) {
    if (current_gain == VEML_GAIN_2) {
        if (current_it == VEML_IT_800MS) return 0.0036f;
        if (current_it == VEML_IT_400MS) return 0.0072f;
        if (current_it == VEML_IT_200MS) return 0.0144f;
        if (current_it == VEML_IT_100MS) return 0.0288f;
        if (current_it == VEML_IT_50MS)  return 0.0576f;
        return 0.1152f;   // 25ms
    }
    if (current_gain == VEML_GAIN_1) {
        if (current_it == VEML_IT_800MS) return 0.0072f;
        if (current_it == VEML_IT_400MS) return 0.0144f;
        if (current_it == VEML_IT_200MS) return 0.0288f;
        if (current_it == VEML_IT_100MS) return 0.0576f;
        if (current_it == VEML_IT_50MS)  return 0.1152f;
        return 0.2304f;   // 25ms
    }
    if (current_gain == VEML_GAIN_1_4) {
        if (current_it == VEML_IT_800MS) return 0.0288f;
        return 0.9216f;
    }
    // GAIN_1_8
    return 1.8432f;
}

// ── Write config register ─────────────────────────────────────

static esp_err_t write_config(uint16_t gain, uint8_t it) {
    uint16_t config = gain | it | VEML_POWER_ON;
    uint8_t  buf[2];
    buf[0] = config & 0xFF;        // VEML7700 is little endian
    buf[1] = (config >> 8) & 0xFF;
    current_gain = gain;
    current_it   = it;
    return i2c_driver_write(VEML7700_ADDR,
                             VEML_REG_CONFIG,
                             buf, 2);
}

// ── Read a 16 bit little endian register ──────────────────────
// Note: VEML7700 is little endian unlike most sensors

static esp_err_t read_word(uint8_t reg, uint16_t *value) {
    uint8_t buf[2] = {0};
    esp_err_t ret  = i2c_driver_read(VEML7700_ADDR,
                                      reg, buf, 2);
    if (ret != ESP_OK) return ret;
    *value = ((uint16_t)buf[1] << 8) | buf[0];  // little endian
    return ESP_OK;
}

// ── Auto range ────────────────────────────────────────────────
// Adjusts gain and integration time to keep count in good range

static void auto_range(uint16_t count) {

    if (count > 60000) {
        // Too bright — reduce sensitivity
        if (current_it == VEML_IT_800MS) {
            write_config(current_gain, VEML_IT_400MS);
        } else if (current_it == VEML_IT_400MS) {
            write_config(current_gain, VEML_IT_200MS);
        } else if (current_it == VEML_IT_200MS) {
            write_config(current_gain, VEML_IT_100MS);
        } else if (current_it == VEML_IT_100MS) {
            write_config(current_gain, VEML_IT_50MS);
        } else if (current_it == VEML_IT_50MS) {
            write_config(current_gain, VEML_IT_25MS);
        } else if (current_gain == VEML_GAIN_2) {
            write_config(VEML_GAIN_1, VEML_IT_25MS);
        } else if (current_gain == VEML_GAIN_1) {
            write_config(VEML_GAIN_1_4, VEML_IT_25MS);
        } else {
            write_config(VEML_GAIN_1_8, VEML_IT_25MS);
        }

        // Wait for new integration period
        vTaskDelay(pdMS_TO_TICKS(110));
    }

    else if (count < 100) {
        // Too dark — increase sensitivity
        if (current_gain == VEML_GAIN_1_8) {
            write_config(VEML_GAIN_1_4, current_it);
        } else if (current_gain == VEML_GAIN_1_4) {
            write_config(VEML_GAIN_1,   current_it);
        } else if (current_gain == VEML_GAIN_1) {
            write_config(VEML_GAIN_2,   current_it);
        } else if (current_it == VEML_IT_25MS) {
            write_config(current_gain, VEML_IT_50MS);
        } else if (current_it == VEML_IT_50MS) {
            write_config(current_gain, VEML_IT_100MS);
        } else if (current_it == VEML_IT_100MS) {
            write_config(current_gain, VEML_IT_200MS);
        } else if (current_it == VEML_IT_200MS) {
            write_config(current_gain, VEML_IT_400MS);
        } else {
            write_config(current_gain, VEML_IT_800MS);
        }

        vTaskDelay(pdMS_TO_TICKS(110));
    }
}

// ── Map lux to display brightness 0-255 ──────────────────────

static uint8_t lux_to_brightness(float lux) {
    if (lux < VEML_LUX_DARK)    return 10;    // minimum visible
    if (lux < VEML_LUX_DIM)     return 40;
    if (lux < VEML_LUX_INDOOR)  return 100;
    if (lux < VEML_LUX_OUTDOOR) return 180;
    if (lux < VEML_LUX_BRIGHT)  return 220;
    return 255;                                // maximum
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t veml7700_init(void) {

    // Start with gain x1, 100ms integration time
    esp_err_t ret = write_config(VEML_GAIN_1, VEML_IT_100MS);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_LIGHT, "VEML7700 not found on I2C bus");
        return ret;
    }

    // Wait one integration period before first read
    vTaskDelay(pdMS_TO_TICKS(110));

    // Take initial reading
    veml7700_data_t data = {0};
    veml7700_read(&data);

    LOG_INFO(TAG_LIGHT,
             "VEML7700 initialized — %.1f lux  brightness %d",
             data.lux, data.brightness);

    return ESP_OK;
}

// ── Read ──────────────────────────────────────────────────────

esp_err_t veml7700_read(veml7700_data_t *data) {

    // Read raw ALS count
    esp_err_t ret = read_word(VEML_REG_ALS, &data->raw_als);
    if (ret != ESP_OK) return ret;

    // Read white channel
    read_word(VEML_REG_WHITE, &data->raw_white);

    // Check saturation
    data->saturated = (data->raw_als >= 65535);

    // Auto range if needed then recalculate
    if (data->raw_als > 60000 || data->raw_als < 100) {
        auto_range(data->raw_als);
        read_word(VEML_REG_ALS, &data->raw_als);
    }

    // Calculate lux using resolution for current settings
    data->lux = data->raw_als * get_resolution();

    // Apply polynomial correction for accuracy above 1000 lux
    if (data->lux > 1000.0f) {
        data->lux = 6.0135e-13f * powf(data->lux, 4)
                  - 9.3924e-9f  * powf(data->lux, 3)
                  + 8.1488e-5f  * powf(data->lux, 2)
                  + 1.0023f     * data->lux;
    }

    data->brightness = lux_to_brightness(data->lux);

    return ESP_OK;
}

// ── Simple getters ────────────────────────────────────────────

float veml7700_get_lux(void) {
    veml7700_data_t data = {0};
    veml7700_read(&data);
    return data.lux;
}

uint8_t veml7700_get_brightness(void) {
    veml7700_data_t data = {0};
    veml7700_read(&data);
    return data.brightness;
}

// ── Shutdown ──────────────────────────────────────────────────

esp_err_t veml7700_shutdown(void) {
    uint8_t buf[2] = {VEML_POWER_OFF, 0x00};
    LOG_INFO(TAG_LIGHT, "VEML7700 shutdown");
    return i2c_driver_write(VEML7700_ADDR,
                             VEML_REG_CONFIG,
                             buf, 2);
}

// ── Wake up ───────────────────────────────────────────────────

esp_err_t veml7700_wakeup(void) {
    esp_err_t ret = write_config(VEML_GAIN_1, VEML_IT_100MS);
    vTaskDelay(pdMS_TO_TICKS(110));
    LOG_INFO(TAG_LIGHT, "VEML7700 wakeup");
    return ret;
}