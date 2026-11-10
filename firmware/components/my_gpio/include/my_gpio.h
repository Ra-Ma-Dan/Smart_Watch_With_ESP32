#ifndef MY_GPIO_H
#define MY_GPIO_H

#include "driver/gpio.h"

// ── Pin definitions ──────────────────────────────────────────

// Display
#define PIN_DISPLAY_DC    GPIO_NUM_4
#define PIN_DISPLAY_RST   GPIO_NUM_5
#define PIN_DISPLAY_CS    GPIO_NUM_6
#define PIN_SPI_MOSI      GPIO_NUM_7
#define PIN_SPI_CLK       GPIO_NUM_15

// I2C
#define PIN_I2C_SDA       GPIO_NUM_8
#define PIN_I2C_SCL       GPIO_NUM_9

// Interrupt lines
#define PIN_TOUCH_INT     GPIO_NUM_10
#define PIN_BMI270_INT    GPIO_NUM_11
#define PIN_AXP2101_IRQ   GPIO_NUM_12

// Buttons
#define PIN_SOS_BUTTON    GPIO_NUM_0

// ── Hold detection ───────────────────────────────────────────
#define SOS_HOLD_MS       3000   // 3 seconds to trigger SOS

// ── Callback types ───────────────────────────────────────────

// Functions you pass in to handle events
typedef void (*gpio_callback_t)(void);

// ── Function declarations ─────────────────────────────────────

void gpio_driver_init(void);

// Output control
void gpio_driver_set_high(gpio_num_t pin);
void gpio_driver_set_low(gpio_num_t pin);

// Input reading
int  gpio_driver_read(gpio_num_t pin);

// Interrupt registration
void gpio_driver_register_sos_callback(gpio_callback_t cb);
void gpio_driver_register_touch_callback(gpio_callback_t cb);
void gpio_driver_register_bmi270_callback(gpio_callback_t cb);
void gpio_driver_register_axp2101_callback(gpio_callback_t cb);

#endif