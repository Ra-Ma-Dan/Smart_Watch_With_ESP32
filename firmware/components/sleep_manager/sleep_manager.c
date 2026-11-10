#include "sleep_manager.h"
#include "my_uart.h"
#include "gpio_driver.h"
#include "gc9a01.h"
#include "axp2101.h"
#include "tmp117.h"
#include "veml7700.h"
#include "max30102.h"
#include "bmi270.h"
#include "drv2605l.h"
#include "ble_driver.h"
#include "lvgl_port.h"
#include "ui_watchface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include <string.h>

static power_state_t  current_state   = POWER_STATE_ACTIVE;
static wake_reason_t  last_wake       = WAKE_REASON_UNKNOWN;
static bool           display_on      = true;
static TimerHandle_t  active_timer    = NULL;
static TimerHandle_t  light_timer     = NULL;

#define PIN_BACKLIGHT    GPIO_NUM_16

// External pin definitions from your custom gpio definitions mapping
#define PIN_TOUCH_INT    GPIO_NUM_4
#define PIN_BMI270_INT   GPIO_NUM_5
#define PIN_AXP2101_IRQ  GPIO_NUM_6

static void backlight_on(void) {
    gpio_set_level(PIN_BACKLIGHT, 1);
    display_on = true;
}

static void backlight_off(void) {
    gpio_set_level(PIN_BACKLIGHT, 0);
    display_on = false;
}

static void active_timeout_cb(TimerHandle_t timer) {
    LOG_INFO(TAG_SYSTEM, "Watchdog active state timed out, jumping to light sleep");
    sleep_manager_enter_light_sleep();
}

static void light_timeout_cb(TimerHandle_t timer) {
    LOG_INFO(TAG_SYSTEM, "Extended inactivity, pushing system to deep sleep profile");
    sleep_manager_enter_deep_sleep();
}

static void configure_light_sleep_wakeup(void) {
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable(PIN_TOUCH_INT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(PIN_BMI270_INT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(PIN_AXP2101_IRQ, GPIO_INTR_LOW_LEVEL);
}

static void configure_deep_sleep_wakeup(void) {
    esp_sleep_enable_ext0_wakeup(PIN_BMI270_INT, 0); // 0 = Low Level Wake
    esp_sleep_enable_ext1_wakeup((1ULL << PIN_AXP2101_IRQ), ESP_EXT1_WAKEUP_ANY_LOW);
}

esp_err_t sleep_manager_enter_light_sleep(void) {
    if (current_state == POWER_STATE_LIGHT_SLEEP) return ESP_OK;

    xTimerStop(active_timer, 0);
    backlight_off();

    lvgl_port_lock();
    lv_disp_set_bg_color(lv_disp_get_default(), lv_color_black());
    lvgl_port_unlock();

    tmp117_shutdown();
    veml7700_shutdown();
    max30102_shutdown();
    drv2605l_standby(true);
    bmi270_set_power_mode(true);

    current_state = POWER_STATE_LIGHT_SLEEP;
    xTimerStart(light_timer, 0);
    configure_light_sleep_wakeup();

    esp_light_sleep_start();

    // Execution block resumes here instantly on low-power waking interrupt trigger
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    wake_reason_t reason = WAKE_REASON_UNKNOWN;

    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        if (!gpio_get_level(PIN_TOUCH_INT))       reason = WAKE_REASON_TOUCH;
        else if (!gpio_get_level(PIN_BMI270_INT)) reason = WAKE_REASON_WRIST_RAISE;
        else if (!gpio_get_level(PIN_AXP2101_IRQ)) reason = WAKE_REASON_BUTTON;
    }

    sleep_manager_wake_up(reason);
    return ESP_OK;
}

esp_err_t sleep_manager_enter_deep_sleep(void) {
    LOG_INFO(TAG_SYSTEM, "Halting background processing for total shutdown");
    xTimerStop(active_timer, 0);
    xTimerStop(light_timer,  0);

    backlight_off();
    tmp117_shutdown();
    veml7700_shutdown();
    max30102_shutdown();
    drv2605l_standby(true);
    bmi270_set_power_mode(true);
    ble_driver_stop_advertising();

    axp2101_enable_rail(AXP_REG_PWR_EN1, 0, false);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 1, false);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 2, false);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 4, false);

    current_state = POWER_STATE_DEEP_SLEEP;
    configure_deep_sleep_wakeup();

    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
    return ESP_OK; 
}

esp_err_t sleep_manager_wake_up(wake_reason_t reason) {
    last_wake     = reason;
    current_state = POWER_STATE_ACTIVE;

    axp2101_enable_rail(AXP_REG_PWR_EN1, 0, true);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 1, true);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 2, true);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 4, true);
    vTaskDelay(pdMS_TO_TICKS(10));

    veml7700_wakeup();
    max30102_wakeup();
    drv2605l_standby(false);
    bmi270_set_power_mode(false);
    backlight_on();

    lvgl_port_lock();
    lv_obj_invalidate(lv_scr_act());
    lvgl_port_unlock();

    switch (reason) {
        case WAKE_REASON_TOUCH:
            drv2605l_play_pattern(HAPTIC_PATTERN_BUTTON_CLICK);
            break;
        case WAKE_REASON_BUTTON:
            drv2605l_play_pattern(HAPTIC_PATTERN_BUTTON_CLICK);
            break;
        case WAKE_REASON_FALL:
            drv2605l_play_pattern(HAPTIC_PATTERN_SOS);
            ble_driver_notify_sos();
            break;
        default:
            break;
    }

    xTimerReset(active_timer, 0);
    return ESP_OK;
}

void sleep_manager_feed_watchdog(void) {
    if (current_state != POWER_STATE_ACTIVE) {
        sleep_manager_wake_up(WAKE_REASON_TOUCH);
    } else {
        xTimerReset(active_timer, 0);
    }
}

power_state_t sleep_manager_get_state(void) { return current_state; }
wake_reason_t sleep_manager_get_wake_reason(void) { return last_wake; }
bool sleep_manager_is_display_on(void) { return display_on; }

esp_err_t sleep_manager_init(void) {
    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << PIN_BACKLIGHT),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&bl_conf);
    backlight_on();

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT0 || cause == ESP_SLEEP_WAKEUP_EXT1) {
        last_wake = (cause == ESP_SLEEP_WAKEUP_EXT0) ? WAKE_REASON_WRIST_RAISE : WAKE_REASON_BUTTON;
    }

    active_timer = xTimerCreate("act_tmr", pdMS_TO_TICKS(SLEEP_ACTIVE_TIMEOUT_MS), pdFALSE, NULL, active_timeout_cb);
    light_timer  = xTimerCreate("lgt_tmr", pdMS_TO_TICKS(SLEEP_LIGHT_TIMEOUT_MS), pdFALSE, NULL, light_timeout_cb);

    if (active_timer == NULL || light_timer == NULL) return ESP_ERR_NO_MEM;

    xTimerStart(active_timer, 0);
    return ESP_OK;
}
