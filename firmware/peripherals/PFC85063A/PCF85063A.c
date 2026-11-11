#include "PCF85063A.h"

// ── BCD helpers ───────────────────────────────────────────────

static uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t pcf85063a_init(void) {

    // Configure control register 1
    // 24hr mode, oscillator enabled, no correction
    esp_err_t ret = i2c_driver_write_byte(PCF85063A_ADDR,
                                           PCF_REG_CTRL1,
                                           0x00);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_RTC, "PCF85063A not found on I2C bus");
        return ret;
    }

    // Configure control register 2
    // Interrupt on alarm match, no timer interrupt
    i2c_driver_write_byte(PCF85063A_ADDR, PCF_REG_CTRL2, 0x00);

    // Check oscillator stop flag
    if (!pcf85063a_is_valid()) {
        LOG_WARN(TAG_RTC,
                 "Oscillator stop flag set — time is invalid");
        LOG_WARN(TAG_RTC,
                 "Sync time via BLE or set manually");

        // Set a default time so watch is not stuck at 00:00:00
        // Real time will be synced from phone via BLE later
        pcf85063a_time_t default_time = {
            .seconds = 0,
            .minutes = 0,
            .hours   = 0,
            .days    = 1,
            .weekday = 0,
            .months  = 1,
            .years   = 25    // 2025
        };
        pcf85063a_set_time(&default_time);
    } else {
        // Time is valid, log it
        pcf85063a_time_t now = {0};
        pcf85063a_get_time(&now);
        LOG_INFO(TAG_RTC, "PCF85063A initialized — time valid");
        pcf85063a_log_time(&now);
    }

    return ESP_OK;
}

// ── Check oscillator stop flag ────────────────────────────────

bool pcf85063a_is_valid(void) {
    uint8_t seconds = 0;
    i2c_driver_read_byte(PCF85063A_ADDR,
                          PCF_REG_SECONDS,
                          &seconds);

    // OS flag is bit 7 of seconds register
    // 1 = time invalid, 0 = time valid
    return (seconds & PCF_OS_FLAG) == 0;
}

// ── Set time ──────────────────────────────────────────────────

esp_err_t pcf85063a_set_time(const pcf85063a_time_t *time) {

    // Write all 7 time registers in one burst
    // PCF85063A auto increments register address
    uint8_t buf[7];
    buf[0] = dec_to_bcd(time->seconds) & ~PCF_OS_FLAG; // clear OS flag
    buf[1] = dec_to_bcd(time->minutes);
    buf[2] = dec_to_bcd(time->hours);
    buf[3] = dec_to_bcd(time->days);
    buf[4] = time->weekday & 0x07;
    buf[5] = dec_to_bcd(time->months);
    buf[6] = dec_to_bcd(time->years);

    esp_err_t ret = i2c_driver_write(PCF85063A_ADDR,
                                      PCF_REG_SECONDS,
                                      buf, 7);
    if (ret == ESP_OK) {
        LOG_INFO(TAG_RTC, "Time set successfully");
        pcf85063a_log_time(time);
    }

    return ret;
}

// ── Get time ──────────────────────────────────────────────────

esp_err_t pcf85063a_get_time(pcf85063a_time_t *time) {

    // Read all 7 time registers in one burst
    uint8_t buf[7] = {0};
    esp_err_t ret = i2c_driver_read(PCF85063A_ADDR,
                                     PCF_REG_SECONDS,
                                     buf, 7);
    if (ret != ESP_OK) return ret;

    // Decode BCD — mask off control bits first
    time->seconds = bcd_to_dec(buf[0] & 0x7F);  // mask OS flag
    time->minutes = bcd_to_dec(buf[1] & 0x7F);
    time->hours   = bcd_to_dec(buf[2] & 0x3F);  // mask 12/24 bit
    time->days    = bcd_to_dec(buf[3] & 0x3F);
    time->weekday = buf[4] & 0x07;
    time->months  = bcd_to_dec(buf[5] & 0x1F);
    time->years   = bcd_to_dec(buf[6]);

    return ESP_OK;
}

// ── Set alarm ─────────────────────────────────────────────────

esp_err_t pcf85063a_set_alarm(const pcf85063a_time_t *alarm) {

    // Bit 7 of each alarm register = 0 means enabled
    // Bit 7 = 1 means that field is ignored
    uint8_t buf[5];
    buf[0] = dec_to_bcd(alarm->seconds) & 0x7F;   // enable seconds match
    buf[1] = dec_to_bcd(alarm->minutes) & 0x7F;   // enable minutes match
    buf[2] = dec_to_bcd(alarm->hours)   & 0x7F;   // enable hours match
    buf[3] = 0x80;                                  // ignore day
    buf[4] = 0x80;                                  // ignore weekday

    esp_err_t ret = i2c_driver_write(PCF85063A_ADDR,
                                      PCF_REG_ALARM_SEC,
                                      buf, 5);

    if (ret == ESP_OK) {
        // Enable alarm interrupt in control register 2
        uint8_t ctrl2 = 0;
        i2c_driver_read_byte(PCF85063A_ADDR,
                              PCF_REG_CTRL2,
                              &ctrl2);
        ctrl2 |= (1 << 7);   // alarm interrupt enable
        i2c_driver_write_byte(PCF85063A_ADDR,
                               PCF_REG_CTRL2,
                               ctrl2);

        LOG_INFO(TAG_RTC, "Alarm set for %02d:%02d:%02d",
                 alarm->hours,
                 alarm->minutes,
                 alarm->seconds);
    }

    return ret;
}

// ── Clear alarm ───────────────────────────────────────────────

esp_err_t pcf85063a_clear_alarm(void) {
    uint8_t ctrl2 = 0;
    i2c_driver_read_byte(PCF85063A_ADDR, PCF_REG_CTRL2, &ctrl2);
    ctrl2 &= ~(1 << 6);   // clear alarm flag
    return i2c_driver_write_byte(PCF85063A_ADDR,
                                  PCF_REG_CTRL2,
                                  ctrl2);
}

// ── Log time to debug ─────────────────────────────────────────

void pcf85063a_log_time(const pcf85063a_time_t *time) {
    LOG_INFO(TAG_RTC,
             "%s %04d-%02d-%02d  %02d:%02d:%02d",
             PCF_WEEKDAY_NAMES[time->weekday],
             2000 + time->years,
             time->months,
             time->days,
             time->hours,
             time->minutes,
             time->seconds);
}