#ifndef PCF85063A_H
#define PCF85063A_H

#include "i2c_driver.h"
#include "uart_driver.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define PCF85063A_ADDR        I2C_ADDR_PCF85063A   // 0x51

// ── Registers ─────────────────────────────────────────────────
#define PCF_REG_CTRL1         0x00
#define PCF_REG_CTRL2         0x01
#define PCF_REG_SECONDS       0x02
#define PCF_REG_MINUTES       0x03
#define PCF_REG_HOURS         0x04
#define PCF_REG_DAYS          0x05
#define PCF_REG_WEEKDAY       0x06
#define PCF_REG_MONTHS        0x07
#define PCF_REG_YEARS         0x08
#define PCF_REG_ALARM_SEC     0x09
#define PCF_REG_ALARM_MIN     0x0A
#define PCF_REG_ALARM_HOUR    0x0B
#define PCF_REG_ALARM_DAY     0x0C
#define PCF_REG_ALARM_WDAY    0x0D
#define PCF_REG_TIMER_VAL     0x0E
#define PCF_REG_TIMER_MODE    0x0F

// ── Oscillator stop flag ──────────────────────────────────────
#define PCF_OS_FLAG           (1 << 7)

// ── Time struct ───────────────────────────────────────────────
typedef struct {
    uint8_t seconds;    // 0-59
    uint8_t minutes;    // 0-59
    uint8_t hours;      // 0-23
    uint8_t days;       // 1-31
    uint8_t weekday;    // 0=Sunday, 6=Saturday
    uint8_t months;     // 1-12
    uint8_t years;      // 0-99 (2000-2099)
} pcf85063a_time_t;

// ── Weekday names for logging ─────────────────────────────────
static const char *PCF_WEEKDAY_NAMES[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

// ── Function declarations ─────────────────────────────────────
esp_err_t  pcf85063a_init(void);
esp_err_t  pcf85063a_set_time(const pcf85063a_time_t *time);
esp_err_t  pcf85063a_get_time(pcf85063a_time_t *time);
bool       pcf85063a_is_valid(void);
esp_err_t  pcf85063a_set_alarm(const pcf85063a_time_t *alarm);
esp_err_t  pcf85063a_clear_alarm(void);
void       pcf85063a_log_time(const pcf85063a_time_t *time);

#endif