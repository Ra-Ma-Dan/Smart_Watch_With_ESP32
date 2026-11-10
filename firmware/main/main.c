#include "my_uart.h"
#include "my_gpio.h"
#include "my_i2c.h"
#include "my_spi.h"
#include "gc9a01.h"
#include "axp2101.h"
#include "max17048.h"
#include "pcf85063a.h"
#include "tmp117.h"
#include "veml7700.h"
#include "max30102.h"
#include "bmi270.h"
#include "cst816s.h"
#include "drv2605l.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// ── Forward declarations ──────────────────────────────────────
static void clock_task(void *arg);
static void battery_task(void *arg);
static void temp_task(void *arg);
static void light_task(void *arg);
static void heart_rate_task(void *arg);
static void imu_task(void *arg);

// ── Event handlers ────────────────────────────────────────────

void on_axp2101_irq(void)  { axp2101_handle_irq();      }
void on_touch_interrupt(void) { cst816s_handle_interrupt(); }
void on_bmi270_interrupt(void) {
    bmi270_data_t data = {0};
    bmi270_handle_interrupt(&data);
    if (data.fall_detected) {
        drv2605l_play_pattern(HAPTIC_PATTERN_SOS);
        LOG_WARN(TAG_SYSTEM, "Fall detected — SOS triggered");
    }
    if (data.wrist_raised) {
        LOG_INFO(TAG_SYSTEM, "Wrist raised — display wake");
    }
}

void on_power_event(axp2101_btn_event_t event,
                    axp2101_status_t    status) {
    if (event == AXP_BTN_SHORT_PRESS) {
        drv2605l_play_pattern(HAPTIC_PATTERN_BUTTON_CLICK);
        LOG_INFO(TAG_POWER, "Short press");
    }
    if (event == AXP_BTN_LONG_PRESS) {
        drv2605l_play_pattern(HAPTIC_PATTERN_WARNING);
        gc9a01_fill_screen(COLOR_BLACK);
        gc9a01_draw_string(70, 115,
                           "Goodbye",
                           COLOR_WHITE, COLOR_BLACK, 2);
        vTaskDelay(pdMS_TO_TICKS(1000));
        axp2101_ship_mode();
    }
}

void on_sos_button(void) {
    drv2605l_play_pattern(HAPTIC_PATTERN_SOS);
    LOG_WARN(TAG_SYSTEM, "SOS button held — sending alert");
}

void on_touch(cst816s_touch_t touch) {
    drv2605l_play_pattern(HAPTIC_PATTERN_SWIPE);
    LOG_INFO(TAG_TOUCH, "%s at (%d,%d)",
             cst816s_gesture_name(touch.gesture),
             touch.x, touch.y);
}

// ── Boot sequence ─────────────────────────────────────────────

void app_main(void) {

    // Infrastructure
    uart_driver_init(LOG_LEVEL_INFO);
    uart_driver_log_system_info();
    gpio_driver_init();
    ESP_ERROR_CHECK(i2c_driver_init());
    i2c_driver_scan();

    // Power first
    ESP_ERROR_CHECK(axp2101_init());
    axp2101_register_callback(on_power_event);
    gpio_driver_register_axp2101_callback(on_axp2101_irq);

    // Battery
    ESP_ERROR_CHECK(max17048_init());

    // Time
    ESP_ERROR_CHECK(pcf85063a_init());

    // Display
    ESP_ERROR_CHECK(gc9a01_init());

    // Boot screen
    gc9a01_fill_screen(COLOR_BLACK);
    gc9a01_draw_circle(120, 120, 118, COLOR_WHITE);
    gc9a01_draw_circle(120, 120, 116, COLOR_WHITE);
    gc9a01_draw_string(55, 100,
                       "AH WATCH",
                       COLOR_WHITE, COLOR_BLACK, 2);
    gc9a01_draw_string(75, 125,
                       "Loading...",
                       COLOR_CYAN,  COLOR_BLACK, 1);

    // Sensors
    ESP_ERROR_CHECK(tmp117_init());
    ESP_ERROR_CHECK(veml7700_init());
    ESP_ERROR_CHECK(max30102_init());
    ESP_ERROR_CHECK(bmi270_init());

    // Input
    ESP_ERROR_CHECK(cst816s_init());
    cst816s_register_callback(on_touch);
    gpio_driver_register_touch_callback(on_touch_interrupt);
    gpio_driver_register_bmi270_callback(on_bmi270_interrupt);
    gpio_driver_register_sos_callback(on_sos_button);

    // Haptics last — confirm everything is alive with a click
    ESP_ERROR_CHECK(drv2605l_init());
    vTaskDelay(pdMS_TO_TICKS(500));
    drv2605l_play_pattern(HAPTIC_PATTERN_SUCCESS);

    // Clear boot screen
    vTaskDelay(pdMS_TO_TICKS(500));
    gc9a01_fill_screen(COLOR_BLACK);
    gc9a01_draw_circle(120, 120, 118, COLOR_WHITE);
    gc9a01_draw_circle(120, 120, 116, COLOR_WHITE);

    // Start all tasks
    xTaskCreate(clock_task,      "clock",   2048, NULL, 5, NULL);
    xTaskCreate(battery_task,    "battery", 2048, NULL, 3, NULL);
    xTaskCreate(temp_task,       "temp",    2048, NULL, 2, NULL);
    xTaskCreate(light_task,      "light",   2048, NULL, 2, NULL);
    xTaskCreate(heart_rate_task, "hr",      4096, NULL, 2, NULL);
    xTaskCreate(imu_task,        "imu",     3072, NULL, 3, NULL);

    LOG_INFO(TAG_SYSTEM, "AH Watch boot complete");
    LOG_INFO(TAG_SYSTEM, "All 13 drivers initialized");
    LOG_INFO(TAG_SYSTEM, "6 tasks running");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}