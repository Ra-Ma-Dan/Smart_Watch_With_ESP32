#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include "esp_err.h"
#include "esp_sleep.h"
#include <stdint.h>
#include <stdbool.h>

// ── Sleep timeouts ────────────────────────────────────────────
#define SLEEP_ACTIVE_TIMEOUT_MS    10000   // 10s no interaction → light sleep
#define SLEEP_LIGHT_TIMEOUT_MS    120000   // 2min light sleep   → deep sleep

// ── Power states ──────────────────────────────────────────────
typedef enum {
    POWER_STATE_ACTIVE = 0,
    POWER_STATE_LIGHT_SLEEP,
    POWER_STATE_DEEP_SLEEP
} power_state_t;

// ── Wake reason ───────────────────────────────────────────────
typedef enum {
    WAKE_REASON_UNKNOWN = 0,
    WAKE_REASON_TOUCH,
    WAKE_REASON_WRIST_RAISE,
    WAKE_REASON_BUTTON,
    WAKE_REASON_BLE,
    WAKE_REASON_RTC_ALARM,
    WAKE_REASON_FALL
} wake_reason_t;

// ── Function declarations ─────────────────────────────────────
esp_err_t     sleep_manager_init(void);
void          sleep_manager_feed_watchdog(void);
power_state_t sleep_manager_get_state(void);
wake_reason_t sleep_manager_get_wake_reason(void);
esp_err_t     sleep_manager_enter_light_sleep(void);
esp_err_t     sleep_manager_enter_deep_sleep(void);
esp_err_t     sleep_manager_wake_up(wake_reason_t reason);
bool          sleep_manager_is_display_on(void);

#endif