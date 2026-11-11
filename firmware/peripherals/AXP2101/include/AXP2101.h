#ifndef AXP2101_H
#define AXP2101_H

#include "my_uart.h"
#include "my_i2c.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define AXP2101_ADDR         I2C_ADDR_AXP2101   // 0x34

// ── Registers ─────────────────────────────────────────────────
#define AXP_REG_CHIP_ID      0x00
#define AXP_REG_PWR_STATUS   0x01
#define AXP_REG_CHG_STATUS   0x02
#define AXP_REG_DCDC1_VOL    0x20
#define AXP_REG_DCDC2_VOL    0x21
#define AXP_REG_ALDO1_VOL    0x30
#define AXP_REG_ALDO2_VOL    0x31
#define AXP_REG_ALDO3_VOL    0x32
#define AXP_REG_ALDO4_VOL    0x33
#define AXP_REG_BLDO1_VOL    0x35
#define AXP_REG_PWR_EN0      0x90
#define AXP_REG_PWR_EN1      0x91
#define AXP_REG_IRQ_EN0      0x40
#define AXP_REG_IRQ_EN1      0x41
#define AXP_REG_IRQ_STAT0    0x48
#define AXP_REG_IRQ_STAT1    0x49
#define AXP_REG_CHG_CTRL     0x62
#define AXP_REG_BTN_CTRL     0x64

// ── Power status bits ─────────────────────────────────────────
#define AXP_USB_PRESENT      (1 << 5)
#define AXP_BAT_PRESENT      (1 << 3)
#define AXP_CHARGING         (1 << 2)

// ── IRQ bits ──────────────────────────────────────────────────
#define AXP_IRQ_USB_IN       (1 << 5)   // USB plugged in
#define AXP_IRQ_USB_OUT      (1 << 4)   // USB unplugged
#define AXP_IRQ_BTN_SHORT    (1 << 1)   // short button press
#define AXP_IRQ_BTN_LONG     (1 << 0)   // long button press

// ── Power status struct ───────────────────────────────────────
typedef struct {
    bool usb_present;
    bool battery_present;
    bool charging;
} axp2101_status_t;

// ── Button event enum ─────────────────────────────────────────
typedef enum {
    AXP_BTN_NONE = 0,
    AXP_BTN_SHORT_PRESS,
    AXP_BTN_LONG_PRESS
} axp2101_btn_event_t;

// ── Callback type ─────────────────────────────────────────────
typedef void (*axp2101_event_cb_t)(axp2101_btn_event_t event,
                                    axp2101_status_t   status);

// ── Function declarations ─────────────────────────────────────
esp_err_t           axp2101_init(void);
esp_err_t           axp2101_read_status(axp2101_status_t *status);
esp_err_t           axp2101_set_rail_voltage(uint8_t  vol_reg,
                                              uint16_t mv);
esp_err_t           axp2101_enable_rail(uint8_t en_reg,
                                         uint8_t bit,
                                         bool    enable);
axp2101_btn_event_t axp2101_read_button(void);
esp_err_t           axp2101_handle_irq(void);
void                axp2101_register_callback(axp2101_event_cb_t cb);
esp_err_t           axp2101_set_charge_current(uint16_t ma);
esp_err_t           axp2101_ship_mode(void);

#endif