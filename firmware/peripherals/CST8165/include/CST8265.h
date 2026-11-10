#ifndef CST816S_H
#define CST816S_H

#include "i2c_driver.h"
#include "gpio_driver.h"
#include "uart_driver.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define CST816S_ADDR            I2C_ADDR_CST816S   // 0x15

// ── Registers ─────────────────────────────────────────────────
#define CST_REG_GESTURE         0x00
#define CST_REG_FINGER_NUM      0x01
#define CST_REG_X_HIGH          0x02
#define CST_REG_X_LOW           0x03
#define CST_REG_Y_HIGH          0x04
#define CST_REG_Y_LOW           0x05
#define CST_REG_SLEEP           0xA5
#define CST_REG_IRQ_PULSE       0xA7
#define CST_REG_CHIP_ID         0xEC
#define CST_REG_FIRMWARE        0xFA

// ── Chip ID ───────────────────────────────────────────────────
#define CST816S_CHIP_ID         0xB5

// ── Gesture codes ─────────────────────────────────────────────
typedef enum {
    CST_GESTURE_NONE        = 0x00,
    CST_GESTURE_SWIPE_UP    = 0x01,
    CST_GESTURE_SWIPE_DOWN  = 0x02,
    CST_GESTURE_SWIPE_LEFT  = 0x03,
    CST_GESTURE_SWIPE_RIGHT = 0x04,
    CST_GESTURE_SINGLE_TAP  = 0x05,
    CST_GESTURE_DOUBLE_TAP  = 0x0B,
    CST_GESTURE_LONG_PRESS  = 0x0C
} cst816s_gesture_t;

// ── Touch event struct ────────────────────────────────────────
typedef struct {
    uint16_t          x;            // 0-239
    uint16_t          y;            // 0-239
    uint8_t           fingers;      // number of fingers
    cst816s_gesture_t gesture;      // gesture detected
    bool              touched;      // true if finger is down
} cst816s_touch_t;

// ── Callback type ─────────────────────────────────────────────
typedef void (*cst816s_touch_cb_t)(cst816s_touch_t touch);

// ── Function declarations ─────────────────────────────────────
esp_err_t  cst816s_init(void);
esp_err_t  cst816s_read(cst816s_touch_t *touch);
void       cst816s_register_callback(cst816s_touch_cb_t cb);
void       cst816s_handle_interrupt(void);
esp_err_t  cst816s_sleep(void);
const char *cst816s_gesture_name(cst816s_gesture_t gesture);

#endif