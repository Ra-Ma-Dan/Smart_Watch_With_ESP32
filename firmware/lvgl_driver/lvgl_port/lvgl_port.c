#include "lvgl_port.h"
#include "gc9a01.h"
#include "cst816s.h"
#include "uart_driver.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "LVGL_PORT";

// ── LVGL objects ──────────────────────────────────────────────
static lv_disp_drv_t    disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t       *buf1 = NULL;
static lv_indev_drv_t   touch_drv;

// ── Mutex to protect LVGL from concurrent access ──────────────
static SemaphoreHandle_t lvgl_mutex = NULL;

// ── Display flush callback ────────────────────────────────────
// LVGL calls this when it has pixels ready to send to screen
// This is Bridge 1

static void lvgl_flush_cb(lv_disp_drv_t   *drv,
                           const lv_area_t *area,
                           lv_color_t      *color_map) {

    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;

    // Set the window on GC9A01 to match the dirty area
    gc9a01_set_window(area->x1, area->y1, w, h);

    // Push the pixel buffer over SPI
    // color_map is already in RGB565 format matching GC9A01
    gc9a01_push_pixels((uint8_t*)color_map, w * h * 2);

    // Critical — tell LVGL the flush is done
    // Without this LVGL blocks forever waiting
    lv_disp_flush_ready(drv);
}

// ── Touch read callback ───────────────────────────────────────
// LVGL calls this periodically to check for touch input
// This is Bridge 2

static void lvgl_touch_cb(lv_indev_drv_t  *drv,
                           lv_indev_data_t *data) {

    cst816s_touch_t touch = {0};
    cst816s_read(&touch);

    if (touch.touched) {
        data->point.x = touch.x;
        data->point.y = touch.y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state   = LV_INDEV_STATE_RELEASED;
    }
}

// ── LVGL tick task ────────────────────────────────────────────
// LVGL needs a periodic tick to drive animations and timers

static void lvgl_tick_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(LVGL_TICK_PERIOD_MS));
        lv_tick_inc(LVGL_TICK_PERIOD_MS);
    }
}

// ── LVGL handler task ─────────────────────────────────────────
// Runs lv_timer_handler periodically
// This drives all LVGL rendering and animation

static void lvgl_handler_task(void *arg) {
    while (1) {
        lvgl_port_lock();
        lv_timer_handler();
        lvgl_port_unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ── Lock and unlock ───────────────────────────────────────────
// Any task that updates LVGL objects MUST lock first
// Prevents race conditions between sensor tasks and renderer

void lvgl_port_lock(void) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
}

void lvgl_port_unlock(void) {
    xSemaphoreGive(lvgl_mutex);
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t lvgl_port_init(void) {

    // Initialize LVGL core
    lv_init();

    // Allocate display buffer in PSRAM
    // Full frame buffer = 240 x 240 x 2 bytes = 115,200 bytes
    buf1 = (lv_color_t*)heap_caps_malloc(
               LVGL_BUF_SIZE * sizeof(lv_color_t),
               MALLOC_CAP_SPIRAM);   // allocate in PSRAM

    if (buf1 == NULL) {
        LOG_ERROR(TAG, "Failed to allocate LVGL display buffer");
        return ESP_ERR_NO_MEM;
    }

    LOG_INFO(TAG, "Display buffer allocated in PSRAM — %d bytes",
             LVGL_BUF_SIZE * sizeof(lv_color_t));

    // Initialize draw buffer
    // Single buffer — LVGL renders and flushes alternately
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, LVGL_BUF_SIZE);

    // ── Register display driver ───────────────────────────────
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res    = LVGL_HOR_RES;
    disp_drv.ver_res    = LVGL_VER_RES;
    disp_drv.flush_cb   = lvgl_flush_cb;
    disp_drv.draw_buf   = &draw_buf;
    disp_drv.full_refresh = 1;   // full refresh for round display
    lv_disp_drv_register(&disp_drv);

    LOG_INFO(TAG, "Display driver registered — %dx%d",
             LVGL_HOR_RES, LVGL_VER_RES);

    // ── Register touch input driver ───────────────────────────
    lv_indev_drv_init(&touch_drv);
    touch_drv.type    = LV_INDEV_TYPE_POINTER;
    touch_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&touch_drv);

    LOG_INFO(TAG, "Touch input driver registered");

    // ── Create mutex ──────────────────────────────────────────
    lvgl_mutex = xSemaphoreCreateMutex();
    if (lvgl_mutex == NULL) {
        LOG_ERROR(TAG, "Failed to create LVGL mutex");
        return ESP_ERR_NO_MEM;
    }

    // ── Start LVGL tasks ──────────────────────────────────────
    xTaskCreate(lvgl_tick_task,
                "lvgl_tick",
                1024, NULL, 6, NULL);

    xTaskCreate(lvgl_handler_task,
                "lvgl_handler",
                LVGL_TASK_STACK_SIZE,
                NULL,
                LVGL_TASK_PRIORITY,
                NULL);

    LOG_INFO(TAG, "LVGL initialized successfully");
    return ESP_OK;
}