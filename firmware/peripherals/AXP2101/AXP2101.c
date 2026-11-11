#include "AXP2101.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static axp2101_event_cb_t event_callback = NULL;

// ── Voltage encoding ──────────────────────────────────────────
// AXP2101 encodes voltages as register values
// DCDC1: 1500-3400mV in 100mV steps  → value = (mv - 1500) / 100
// ALDO:  500-3500mV  in 100mV steps  → value = (mv - 500)  / 100

static uint8_t encode_dcdc_voltage(uint16_t mv) {
    if (mv < 1500) mv = 1500;
    if (mv > 3400) mv = 3400;
    return (mv - 1500) / 100;
}

static uint8_t encode_aldo_voltage(uint16_t mv) {
    if (mv < 500)  mv = 500;
    if (mv > 3500) mv = 3500;
    return (mv - 500) / 100;
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t axp2101_init(void) {

    // Confirm chip is present
    uint8_t chip_id = 0;
    esp_err_t ret = i2c_driver_read_byte(AXP2101_ADDR,
                                          AXP_REG_CHIP_ID,
                                          &chip_id);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_POWER, "AXP2101 not found on I2C bus");
        return ret;
    }
    LOG_INFO(TAG_POWER, "AXP2101 found — chip ID 0x%02X", chip_id);

    // ── Configure power rails ─────────────────────────────────

    // DCDC1 → 3.3V for ESP32-S3 and main 3.3V rail
    axp2101_set_rail_voltage(AXP_REG_DCDC1_VOL, 3300);
    axp2101_enable_rail(AXP_REG_PWR_EN0, 0, true);

    // DCDC2 → 1.8V for BMI270 core
    axp2101_set_rail_voltage(AXP_REG_DCDC2_VOL, 1800);
    axp2101_enable_rail(AXP_REG_PWR_EN0, 1, true);

    // ALDO1 → 2.8V for MAX30102 analog
    axp2101_set_rail_voltage(AXP_REG_ALDO1_VOL, 2800);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 0, true);

    // ALDO2 → 3.3V for CST816S touch
    axp2101_set_rail_voltage(AXP_REG_ALDO2_VOL, 3300);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 1, true);

    // ALDO3 → 3.3V for VEML7700 light sensor
    axp2101_set_rail_voltage(AXP_REG_ALDO3_VOL, 3300);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 2, true);

    // ALDO4 → 3.3V for PCF85063A RTC
    axp2101_set_rail_voltage(AXP_REG_ALDO4_VOL, 3300);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 3, true);

    // BLDO1 → 3.3V for DRV2605L haptics
    axp2101_set_rail_voltage(AXP_REG_BLDO1_VOL, 3300);
    axp2101_enable_rail(AXP_REG_PWR_EN1, 4, true);

    LOG_INFO(TAG_POWER, "All power rails configured");

    // ── Configure charging ────────────────────────────────────
    // 600mAh battery — charge at 300mA (safe 0.5C rate)
    axp2101_set_charge_current(300);

    // ── Configure button timing ───────────────────────────────
    // Short press < 1s, long press > 2s
    i2c_driver_write_byte(AXP2101_ADDR, AXP_REG_BTN_CTRL, 0x12);

    // ── Enable interrupts ─────────────────────────────────────
    // USB plug events + button events
    uint8_t irq_en = AXP_IRQ_USB_IN   |
                     AXP_IRQ_USB_OUT  |
                     AXP_IRQ_BTN_SHORT|
                     AXP_IRQ_BTN_LONG;

    i2c_driver_write_byte(AXP2101_ADDR, AXP_REG_IRQ_EN0, irq_en);

    LOG_INFO(TAG_POWER, "AXP2101 initialization complete");
    return ESP_OK;
}

// ── Read power status ─────────────────────────────────────────

esp_err_t axp2101_read_status(axp2101_status_t *status) {
    uint8_t raw = 0;
    esp_err_t ret = i2c_driver_read_byte(AXP2101_ADDR,
                                          AXP_REG_PWR_STATUS,
                                          &raw);
    if (ret != ESP_OK) return ret;

    status->usb_present     = (raw & AXP_USB_PRESENT) != 0;
    status->battery_present = (raw & AXP_BAT_PRESENT) != 0;
    status->charging        = (raw & AXP_CHARGING)    != 0;

    return ESP_OK;
}

// ── Set rail voltage ──────────────────────────────────────────

esp_err_t axp2101_set_rail_voltage(uint8_t vol_reg, uint16_t mv) {
    uint8_t val;

    // DCDC registers use different encoding than ALDO registers
    if (vol_reg == AXP_REG_DCDC1_VOL ||
        vol_reg == AXP_REG_DCDC2_VOL) {
        val = encode_dcdc_voltage(mv);
    } else {
        val = encode_aldo_voltage(mv);
    }

    return i2c_driver_write_byte(AXP2101_ADDR, vol_reg, val);
}

// ── Enable or disable a power rail ───────────────────────────

esp_err_t axp2101_enable_rail(uint8_t en_reg,
                               uint8_t bit,
                               bool    enable) {
    uint8_t current = 0;
    esp_err_t ret = i2c_driver_read_byte(AXP2101_ADDR,
                                          en_reg,
                                          &current);
    if (ret != ESP_OK) return ret;

    if (enable) {
        current |=  (1 << bit);
    } else {
        current &= ~(1 << bit);
    }

    return i2c_driver_write_byte(AXP2101_ADDR, en_reg, current);
}

// ── Read button event ─────────────────────────────────────────

axp2101_btn_event_t axp2101_read_button(void) {
    uint8_t irq = 0;
    i2c_driver_read_byte(AXP2101_ADDR, AXP_REG_IRQ_STAT0, &irq);

    // Clear the IRQ flags by writing 1 to them
    i2c_driver_write_byte(AXP2101_ADDR, AXP_REG_IRQ_STAT0, irq);

    if (irq & AXP_IRQ_BTN_LONG)  return AXP_BTN_LONG_PRESS;
    if (irq & AXP_IRQ_BTN_SHORT) return AXP_BTN_SHORT_PRESS;
    return AXP_BTN_NONE;
}

// ── Handle IRQ from GPIO interrupt ───────────────────────────
// Called by the AXP2101 IRQ callback registered in gpio_driver

esp_err_t axp2101_handle_irq(void) {
    uint8_t  irq0 = 0;
    uint8_t  irq1 = 0;

    i2c_driver_read_byte(AXP2101_ADDR, AXP_REG_IRQ_STAT0, &irq0);
    i2c_driver_read_byte(AXP2101_ADDR, AXP_REG_IRQ_STAT1, &irq1);

    // Clear flags
    i2c_driver_write_byte(AXP2101_ADDR, AXP_REG_IRQ_STAT0, irq0);
    i2c_driver_write_byte(AXP2101_ADDR, AXP_REG_IRQ_STAT1, irq1);

    axp2101_status_t status = {0};
    axp2101_read_status(&status);

    if (irq0 & AXP_IRQ_USB_IN) {
        LOG_INFO(TAG_POWER, "USB-C plugged in — charging started");
    }

    if (irq0 & AXP_IRQ_USB_OUT) {
        LOG_INFO(TAG_POWER, "USB-C unplugged");
    }

    // Determine button event and fire callback
    axp2101_btn_event_t btn = AXP_BTN_NONE;

    if (irq0 & AXP_IRQ_BTN_LONG) {
        btn = AXP_BTN_LONG_PRESS;
        LOG_INFO(TAG_POWER, "Power button — long press");
    } else if (irq0 & AXP_IRQ_BTN_SHORT) {
        btn = AXP_BTN_SHORT_PRESS;
        LOG_INFO(TAG_POWER, "Power button — short press");
    }

    if (event_callback && btn != AXP_BTN_NONE) {
        event_callback(btn, status);
    }

    return ESP_OK;
}

// ── Register event callback ───────────────────────────────────

void axp2101_register_callback(axp2101_event_cb_t cb) {
    event_callback = cb;
}

// ── Set charge current ────────────────────────────────────────

esp_err_t axp2101_set_charge_current(uint16_t ma) {
    // AXP2101 charge current steps: 0=100mA 1=200mA 2=300mA
    // 3=400mA 4=500mA 5=600mA
    uint8_t steps[] = {100, 200, 300, 400, 500, 600};
    uint8_t val = 0;

    for (uint8_t i = 0; i < 6; i++) {
        if (ma >= steps[i]) val = i;
    }

    LOG_INFO(TAG_POWER, "Charge current set to %dmA",
             steps[val]);
    return i2c_driver_write_byte(AXP2101_ADDR,
                                  AXP_REG_CHG_CTRL,
                                  val);
}

// ── Ship mode — cuts all power for storage ────────────────────

esp_err_t axp2101_ship_mode(void) {
    LOG_WARN(TAG_POWER, "Entering ship mode — power cut");
    return i2c_driver_write_byte(AXP2101_ADDR, 0x10, 0x30);
}