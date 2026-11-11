#include "DRV2605L.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ── Preset pattern definitions ────────────────────────────────
// Each pattern is up to 8 effect slots
// 0 in a slot means end of sequence

typedef struct {
    uint8_t      effects[8];
    const char  *name;
} haptic_pattern_def_t;

static const haptic_pattern_def_t patterns[HAPTIC_PATTERN_MAX] = {
    // NOTIFICATION  — single sharp click
    { {HAPTIC_SHARP_CLICK, 0},                    "Notification"  },

    // CALL          — repeating buzz sequence
    { {HAPTIC_BUZZ, HAPTIC_BUZZ, HAPTIC_BUZZ, 0}, "Call"          },

    // ALARM         — strong triple pulse
    { {HAPTIC_ALERT_BUZZ, HAPTIC_TRIPLE_CLICK, HAPTIC_ALERT_BUZZ, 0},
                                                   "Alarm"         },

    // SOS           — long strong pulses
    { {HAPTIC_SOS_PULSE, HAPTIC_SOS_PULSE, HAPTIC_SOS_PULSE, 0},
                                                   "SOS"           },

    // STEP_GOAL     — celebratory double click + buzz
    { {HAPTIC_DOUBLE_CLICK, HAPTIC_SUCCESS, 0},   "Step Goal"     },

    // HEART_RATE_DONE — subtle confirmation
    { {HAPTIC_SOFT_BUMP, 0},                      "HR Done"       },

    // BUTTON_CLICK  — crisp single click
    { {HAPTIC_STRONG_CLICK, 0},                   "Button"        },

    // SWIPE         — light soft bump
    { {HAPTIC_SOFT_BUMP, 0},                      "Swipe"         },

    // SUCCESS       — ramp up feel good
    { {HAPTIC_SUCCESS, 0},                        "Success"       },

    // WARNING       — warning buzz
    { {HAPTIC_WARNING, 0},                        "Warning"       },
};

// ── Init ──────────────────────────────────────────────────────

esp_err_t drv2605l_init(void) {

    // Confirm chip ID
    uint8_t id = 0;
    esp_err_t ret = i2c_driver_read_byte(DRV2605L_ADDR,
                                          DRV_REG_CHIP_ID,
                                          &id);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_HAPTIC, "DRV2605L not found on I2C bus");
        return ret;
    }

    // Mask upper 3 bits — only lower 5 are chip ID
    if ((id & 0x1F) != DRV2605L_CHIP_ID) {
        LOG_ERROR(TAG_HAPTIC,
                  "Wrong chip ID 0x%02X expected 0x%02X",
                  id & 0x1F, DRV2605L_CHIP_ID);
        return ESP_ERR_NOT_FOUND;
    }

    LOG_INFO(TAG_HAPTIC, "DRV2605L found — ID 0x%02X", id);

    // Take out of standby
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_MODE,
                           DRV_MODE_INTERNAL_TRIG);
    vTaskDelay(pdMS_TO_TICKS(1));

    // Select ERM library A — best for small coin motors
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_LIBRARY,
                           DRV_LIBRARY_A);

    // Configure for ERM motor
    // Bit 7 = 0 for ERM, brake factor 2, loop gain medium
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_FEEDBACK,
                           0x36);

    // Control 1 — drive time 1ms, AC couple off
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_CTRL1,
                           0x13);

    // Control 2 — bidir input, 300µs sample time
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_CTRL2,
                           0xF5);

    // Control 3 — ERM open loop, supply compensation
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_CTRL3,
                           0xA0);

    // Rated voltage for 3V coin motor
    // Formula: rated_reg = rated_voltage / 21.33mV
    // 3.0V / 0.02133 = 140 = 0x8C
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_RATED_VOLT,
                           0x8C);

    // Overdrive clamp — maximum voltage
    // Clamp at 3.2V = 3200 / 21.33 = 150 = 0x96
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_OD_CLAMP,
                           0x96);

    // Run auto calibration to tune to your specific motor
    ret = drv2605l_run_calibration();
    if (ret != ESP_OK) {
        LOG_WARN(TAG_HAPTIC,
                 "Calibration failed — using defaults");
    }

    // Test with a subtle click on boot
    drv2605l_play_effect(HAPTIC_SOFT_BUMP);
    vTaskDelay(pdMS_TO_TICKS(200));

    // Go to standby until needed
    drv2605l_standby(true);

    LOG_INFO(TAG_HAPTIC, "DRV2605L initialized");
    return ESP_OK;
}

// ── Set one waveform slot ─────────────────────────────────────

esp_err_t drv2605l_set_waveform(uint8_t slot, uint8_t effect) {
    if (slot > 7) return ESP_ERR_INVALID_ARG;
    return i2c_driver_write_byte(DRV2605L_ADDR,
                                  DRV_REG_WAVESEQ1 + slot,
                                  effect);
}

// ── Trigger playback ──────────────────────────────────────────

esp_err_t drv2605l_go(void) {
    return i2c_driver_write_byte(DRV2605L_ADDR,
                                  DRV_REG_GO, 0x01);
}

// ── Stop playback ─────────────────────────────────────────────

esp_err_t drv2605l_stop(void) {
    return i2c_driver_write_byte(DRV2605L_ADDR,
                                  DRV_REG_GO, 0x00);
}

// ── Play single effect ────────────────────────────────────────

esp_err_t drv2605l_play_effect(uint8_t effect) {

    // Wake from standby
    drv2605l_standby(false);
    vTaskDelay(pdMS_TO_TICKS(1));

    // Load effect into slot 0, end marker in slot 1
    drv2605l_set_waveform(0, effect);
    drv2605l_set_waveform(1, 0);       // end marker

    // Play
    esp_err_t ret = drv2605l_go();

    // Return to standby after effect duration
    // Longest effect is about 1 second
    vTaskDelay(pdMS_TO_TICKS(1000));
    drv2605l_standby(true);

    return ret;
}

// ── Play named pattern ────────────────────────────────────────

esp_err_t drv2605l_play_pattern(haptic_pattern_t pattern) {
    if (pattern >= HAPTIC_PATTERN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    const haptic_pattern_def_t *p = &patterns[pattern];
    LOG_INFO(TAG_HAPTIC, "Playing pattern: %s", p->name);

    // Wake from standby
    drv2605l_standby(false);
    vTaskDelay(pdMS_TO_TICKS(1));

    // Load all 8 slots
    for (uint8_t i = 0; i < 8; i++) {
        drv2605l_set_waveform(i, p->effects[i]);
    }

    // Play
    esp_err_t ret = drv2605l_go();

    // Wait for pattern to finish then standby
    vTaskDelay(pdMS_TO_TICKS(2000));
    drv2605l_standby(true);

    return ret;
}

// ── Standby control ───────────────────────────────────────────

esp_err_t drv2605l_standby(bool enable) {
    uint8_t mode = enable ? DRV_MODE_STANDBY
                          : DRV_MODE_INTERNAL_TRIG;
    return i2c_driver_write_byte(DRV2605L_ADDR,
                                  DRV_REG_MODE, mode);
}

// ── Auto calibration ──────────────────────────────────────────
// Measures your specific motor and tunes the driver to it
// Run once after assembly — results saved in chip registers

esp_err_t drv2605l_run_calibration(void) {
    LOG_INFO(TAG_HAPTIC, "Running auto calibration...");

    // Set calibration mode
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_MODE, 0x07);

    // Trigger calibration
    drv2605l_go();

    // Wait for calibration to complete — up to 1.2 seconds
    vTaskDelay(pdMS_TO_TICKS(1200));

    // Check status
    uint8_t status = 0;
    i2c_driver_read_byte(DRV2605L_ADDR,
                          DRV_REG_STATUS,
                          &status);

    // Bit 3 = diagnostic result, 0 = pass
    if (status & (1 << 3)) {
        LOG_WARN(TAG_HAPTIC,
                 "Calibration diagnostic failed 0x%02X",
                 status);
        return ESP_FAIL;
    }

    LOG_INFO(TAG_HAPTIC, "Calibration complete");

    // Return to internal trigger mode
    i2c_driver_write_byte(DRV2605L_ADDR,
                           DRV_REG_MODE,
                           DRV_MODE_INTERNAL_TRIG);

    return ESP_OK;
}