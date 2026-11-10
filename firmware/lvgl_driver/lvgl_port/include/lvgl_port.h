#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include "lvgl.h"
#include "esp_err.h"
#include <stdint.h>

// ── Display resolution ────────────────────────────────────────
#define LVGL_HOR_RES     240
#define LVGL_VER_RES     240

// ── Buffer size ───────────────────────────────────────────────
// Full frame buffer in PSRAM for smooth rendering
#define LVGL_BUF_SIZE    (LVGL_HOR_RES * LVGL_VER_RES)

// ── LVGL task config ──────────────────────────────────────────
#define LVGL_TASK_STACK_SIZE   8192
#define LVGL_TASK_PRIORITY     4
#define LVGL_TICK_PERIOD_MS    5     // LVGL tick every 5ms

// ── Function declarations ─────────────────────────────────────
esp_err_t lvgl_port_init(void);

// These are called by your sensor tasks to update UI
void lvgl_port_lock(void);
void lvgl_port_unlock(void);

#endif