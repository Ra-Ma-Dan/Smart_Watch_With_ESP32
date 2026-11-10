#include "tmp117.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ── Read 16 bit big endian register ──────────────────────────

static esp_err_t read_word(uint8_t reg, uint16_t *value) {
    uint8_t buf[2] = {0};
    esp_err_t ret  = i2c_driver_read(TMP117_ADDR,
                                      reg, buf, 2);
    if (ret != ESP_OK) return ret;
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return ESP_OK;
}

// ── Write 16 bit big endian register ─────────────────────────

static esp_err_t write_word(uint8_t reg, uint16_t value) {
    uint8_t buf[2];
    buf[0] = value >> 8;
    buf[1] = value & 0xFF;
    return i2c_driver_write(TMP117_ADDR, reg, buf, 2);
}

// ── Convert Celsius to raw register value ────────────────────

static int16_t celsius_to_raw(float celsius) {
    return (int16_t)(celsius / TMP117_RESOLUTION);
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t tmp117_init(void) {

    // Confirm device ID
    uint16_t id = 0;
    esp_err_t ret = read_word(TMP_REG_DEVICE_ID, &id);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_TEMP, "TMP117 not found on I2C bus");
        return ret;
    }

    if (id != TMP117_DEVICE_ID) {
        LOG_ERROR(TAG_TEMP,
                  "Wrong device ID 0x%04X expected 0x%04X",
                  id, TMP117_DEVICE_ID);
        return ESP_ERR_NOT_FOUND;
    }

    LOG_INFO(TAG_TEMP, "TMP117 found — ID 0x%04X", id);

    // Configure for shutdown mode
    // We will trigger one-shot reads on demand
    // This keeps power consumption near zero between readings
    uint16_t config = TMP_MODE_SHUTDOWN |
                      TMP_AVG_8         |   // 8 sample average
                      TMP_CYCLE_125MS;      // 125ms per sample

    ret = write_word(TMP_REG_CONFIG, config);
    if (ret != ESP_OK) return ret;

    // Set alert thresholds
    tmp117_set_alerts(TMP_ALERT_HIGH_C, TMP_ALERT_LOW_C);

    // Take one reading to verify sensor works
    tmp117_trigger_oneshot();
    vTaskDelay(pdMS_TO_TICKS(20));   // wait for conversion

    tmp117_data_t data = {0};
    tmp117_read(&data);

    LOG_INFO(TAG_TEMP,
             "Skin temp: %.2f°C  (%.2f°F)",
             data.celsius,
             data.fahrenheit);

    return ESP_OK;
}

// ── Trigger one-shot conversion ───────────────────────────────

esp_err_t tmp117_trigger_oneshot(void) {
    // Read current config
    uint16_t config = 0;
    read_word(TMP_REG_CONFIG, &config);

    // Clear mode bits then set one-shot
    config &= ~0x0C00;
    config |=  TMP_MODE_ONESHOT;

    return write_word(TMP_REG_CONFIG, config);
}

// ── Check if new data is ready ────────────────────────────────

bool tmp117_data_ready(void) {
    uint16_t config = 0;
    read_word(TMP_REG_CONFIG, &config);

    // Bit 13 is data ready flag
    return (config & (1 << 13)) != 0;
}

// ── Read temperature ──────────────────────────────────────────

esp_err_t tmp117_read(tmp117_data_t *data) {

    // Check config register for flags
    uint16_t config = 0;
    esp_err_t ret = read_word(TMP_REG_CONFIG, &config);
    if (ret != ESP_OK) return ret;

    data->data_ready  = (config & (1 << 13)) != 0;
    data->high_alert  = (config & (1 << 15)) != 0;
    data->low_alert   = (config & (1 << 14)) != 0;

    if (!data->data_ready) {
        LOG_WARN(TAG_TEMP, "Data not ready yet");
        return ESP_ERR_INVALID_STATE;
    }

    // Read result register
    uint16_t raw = 0;
    ret = read_word(TMP_REG_RESULT, &raw);
    if (ret != ESP_OK) return ret;

    // Cast to signed then multiply by resolution
    data->celsius    = (int16_t)raw * TMP117_RESOLUTION;
    data->fahrenheit = data->celsius * 1.8f + 32.0f;

    if (data->high_alert) {
        LOG_WARN(TAG_TEMP,
                 "High temp alert — %.2f°C", data->celsius);
    }

    if (data->low_alert) {
        LOG_WARN(TAG_TEMP,
                 "Low temp alert — poor sensor contact %.2f°C",
                 data->celsius);
    }

    return ESP_OK;
}

// ── Simple getters ────────────────────────────────────────────

float tmp117_get_celsius(void) {
    uint16_t raw = 0;
    read_word(TMP_REG_RESULT, &raw);
    return (int16_t)raw * TMP117_RESOLUTION;
}

float tmp117_get_fahrenheit(void) {
    return tmp117_get_celsius() * 1.8f + 32.0f;
}

// ── Set alert thresholds ──────────────────────────────────────

esp_err_t tmp117_set_alerts(float high_c, float low_c) {
    esp_err_t ret;

    ret = write_word(TMP_REG_HIGH_LIMIT,
                     (uint16_t)celsius_to_raw(high_c));
    if (ret != ESP_OK) return ret;

    ret = write_word(TMP_REG_LOW_LIMIT,
                     (uint16_t)celsius_to_raw(low_c));

    LOG_INFO(TAG_TEMP,
             "Alerts set — high: %.1f°C  low: %.1f°C",
             high_c, low_c);
    return ret;
}

// ── Shutdown ──────────────────────────────────────────────────

esp_err_t tmp117_shutdown(void) {
    uint16_t config = 0;
    read_word(TMP_REG_CONFIG, &config);
    config &= ~0x0C00;
    config |=  TMP_MODE_SHUTDOWN;
    LOG_INFO(TAG_TEMP, "TMP117 shutdown");
    return write_word(TMP_REG_CONFIG, config);
}