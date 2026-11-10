#include "max17048.h"

// ── Read a 16 bit big endian register ─────────────────────────
// MAX17048 sends high byte first then low byte

static esp_err_t read_word(uint8_t reg, uint16_t *value) {
    uint8_t buf[2] = {0};
    esp_err_t ret  = i2c_driver_read(MAX17048_ADDR,
                                      reg, buf, 2);
    if (ret != ESP_OK) return ret;

    // High byte first, low byte second
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return ESP_OK;
}

// ── Write a 16 bit big endian register ────────────────────────

static esp_err_t write_word(uint8_t reg, uint16_t value) {
    uint8_t buf[2];
    buf[0] = value >> 8;      // high byte
    buf[1] = value & 0xFF;    // low byte
    return i2c_driver_write(MAX17048_ADDR, reg, buf, 2);
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t max17048_init(void) {

    // Confirm chip is present by reading version
    uint16_t version = 0;
    esp_err_t ret = read_word(MAX_REG_VERSION, &version);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_FUEL, "MAX17048 not found on I2C bus");
        return ret;
    }
    LOG_INFO(TAG_FUEL, "MAX17048 found — version 0x%04X", version);

    // Quick start — forces ModelGauge to restart
    // with a fresh measurement of the battery
    max17048_quick_start();

    // Set hibernation thresholds
    // Hibernate if voltage change < 31.25mV/hr
    // and SOC change < 0.5%/hr
    write_word(MAX_REG_HIBRT, 0x870A);

    // Set alert threshold
    max17048_set_alert_threshold(MAX_ALERT_THRESHOLD);

    // Read and log initial state
    max17048_data_t data = {0};
    max17048_read(&data);

    LOG_INFO(TAG_FUEL, "Battery: %.0fmV  %.1f%%  %.2f%%/hr",
             data.voltage_mv,
             data.percentage,
             data.charge_rate);

    if (data.percentage < 20.0f) {
        LOG_WARN(TAG_FUEL, "Battery low — %.1f%%", data.percentage);
    }

    return ESP_OK;
}

// ── Read all battery data ─────────────────────────────────────

esp_err_t max17048_read(max17048_data_t *data) {
    uint16_t vcell = 0;
    uint16_t soc   = 0;
    uint16_t crate = 0;
    uint16_t cfg   = 0;

    esp_err_t ret;

    ret = read_word(MAX_REG_VCELL, &vcell);
    if (ret != ESP_OK) return ret;

    ret = read_word(MAX_REG_SOC, &soc);
    if (ret != ESP_OK) return ret;

    ret = read_word(MAX_REG_CRATE, &crate);
    if (ret != ESP_OK) return ret;

    ret = read_word(MAX_REG_CONFIG, &cfg);
    if (ret != ESP_OK) return ret;

    // VCELL: each bit = 1.25mV, value is in upper 12 bits
    data->voltage_mv = (vcell >> 4) * 1.25f;

    // SOC: high byte = whole percent, low byte = 1/256 percent
    data->percentage = (float)(soc >> 8) +
                       (float)(soc & 0xFF) / 256.0f;

    // CRATE: signed, each bit = 0.208%/hr
    // Positive = charging, negative = discharging
    data->charge_rate = (int16_t)crate * 0.208f;
    data->is_charging = data->charge_rate > 0;

    // Alert bit is bit 5 of CONFIG low byte
    data->alert = (cfg & (1 << 5)) != 0;

    return ESP_OK;
}

// ── Individual getters ────────────────────────────────────────

float max17048_get_voltage(void) {
    uint16_t vcell = 0;
    read_word(MAX_REG_VCELL, &vcell);
    return (vcell >> 4) * 1.25f;
}

float max17048_get_percentage(void) {
    uint16_t soc = 0;
    read_word(MAX_REG_SOC, &soc);
    return (float)(soc >> 8) + (float)(soc & 0xFF) / 256.0f;
}

float max17048_get_charge_rate(void) {
    uint16_t crate = 0;
    read_word(MAX_REG_CRATE, &crate);
    return (int16_t)crate * 0.208f;
}

// ── Alert threshold ───────────────────────────────────────────

esp_err_t max17048_set_alert_threshold(uint8_t percent) {
    uint16_t cfg = 0;
    read_word(MAX_REG_CONFIG, &cfg);

    // Alert threshold is in lower 5 bits of CONFIG low byte
    // Value = 32 - threshold percent
    uint8_t threshold = 32 - percent;
    cfg = (cfg & 0xFFE0) | (threshold & 0x1F);

    LOG_INFO(TAG_FUEL, "Alert threshold set to %d%%", percent);
    return write_word(MAX_REG_CONFIG, cfg);
}

// ── Clear alert flag ──────────────────────────────────────────

esp_err_t max17048_clear_alert(void) {
    uint16_t cfg = 0;
    read_word(MAX_REG_CONFIG, &cfg);
    cfg &= ~(1 << 5);    // clear alert bit
    return write_word(MAX_REG_CONFIG, cfg);
}

// ── Hibernation ───────────────────────────────────────────────
// Hibernate when battery is idle to save power
// MAX17048 uses only 3µA in hibernate vs 23µA active

esp_err_t max17048_hibernate(bool enable) {
    uint16_t mode = 0;
    read_word(MAX_REG_MODE, &mode);

    if (enable) {
        mode |=  (1 << 11);   // set ENHIB bit
        LOG_INFO(TAG_FUEL, "Hibernate enabled");
    } else {
        mode &= ~(1 << 11);   // clear ENHIB bit
        LOG_INFO(TAG_FUEL, "Hibernate disabled");
    }

    return write_word(MAX_REG_MODE, mode);
}

// ── Quick start ───────────────────────────────────────────────
// Forces ModelGauge to restart with fresh readings
// Use after power on or if SOC reading looks wrong

esp_err_t max17048_quick_start(void) {
    uint16_t mode = 0;
    read_word(MAX_REG_MODE, &mode);
    mode |= (1 << 14);    // set QSTRT bit
    return write_word(MAX_REG_MODE, mode);
}