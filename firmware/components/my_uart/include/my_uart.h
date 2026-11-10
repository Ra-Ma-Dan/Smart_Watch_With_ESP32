#ifndef MY_UART_H
#define MY_UART_H

#include <stdint.h>
#include "esp_log.h"

// ── Log levels ────────────────────────────────────────────────
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

// ── Module tags ───────────────────────────────────────────────
// Every driver in the project uses one of these tags
// so you can filter output by component on your terminal

#define TAG_SYSTEM    "SYSTEM"
#define TAG_GPIO      "GPIO"
#define TAG_I2C       "I2C"
#define TAG_SPI       "SPI"
#define TAG_DISPLAY   "DISPLAY"
#define TAG_TOUCH     "TOUCH"
#define TAG_HEART     "MAX30102"
#define TAG_IMU       "BMI270"
#define TAG_TEMP      "TMP117"
#define TAG_LIGHT     "VEML7700"
#define TAG_POWER     "AXP2101"
#define TAG_FUEL      "MAX17048"
#define TAG_RTC       "PCF85063A"
#define TAG_HAPTIC    "DRV2605L"
#define TAG_BLE       "BLE"
#define TAG_OTA       "OTA"

// ── Convenience macros ────────────────────────────────────────
// Wrap ESP-IDF logging so every driver calls these
// instead of ESP_LOGI directly

#define LOG_DEBUG(tag, fmt, ...)  ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag,  fmt, ...)  ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag,  fmt, ...)  ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)

// ── Function declarations ─────────────────────────────────────
void uart_driver_init(log_level_t level);
void uart_driver_set_level(log_level_t level);
void uart_driver_log_system_info(void);

#endif
