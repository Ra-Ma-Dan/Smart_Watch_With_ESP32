#ifndef DRV2605L_H
#define DRV2605L_H

#include "my_i2c.h"
#include "my_uart.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define DRV2605L_ADDR           I2C_ADDR_DRV2605L   // 0x5A

// ── Registers ─────────────────────────────────────────────────
#define DRV_REG_STATUS          0x00
#define DRV_REG_MODE            0x01
#define DRV_REG_RTP             0x02
#define DRV_REG_LIBRARY         0x03
#define DRV_REG_WAVESEQ1        0x04
#define DRV_REG_WAVESEQ2        0x05
#define DRV_REG_WAVESEQ3        0x06
#define DRV_REG_WAVESEQ4        0x07
#define DRV_REG_WAVESEQ5        0x08
#define DRV_REG_WAVESEQ6        0x09
#define DRV_REG_WAVESEQ7        0x0A
#define DRV_REG_WAVESEQ8        0x0B
#define DRV_REG_GO              0x0C
#define DRV_REG_RATED_VOLT      0x16
#define DRV_REG_OD_CLAMP        0x17
#define DRV_REG_FEEDBACK        0x1A
#define DRV_REG_CTRL1           0x1B
#define DRV_REG_CTRL2           0x1C
#define DRV_REG_CTRL3           0x1D
#define DRV_REG_CTRL4           0x1E
#define DRV_REG_CHIP_ID         0x22

// ── Chip ID ───────────────────────────────────────────────────
#define DRV2605L_CHIP_ID        0x07

// ── Mode register values ──────────────────────────────────────
#define DRV_MODE_INTERNAL_TRIG  0x00   // play on GO bit
#define DRV_MODE_RTP            0x05   // real time playback
#define DRV_MODE_STANDBY        0x40   // low power standby

// ── Library selection ─────────────────────────────────────────
// Library A = best for ERM motors under 3V rated voltage
// Library B-F = ERM motors at various voltages
// Library 6   = LRA motors

#define DRV_LIBRARY_EMPTY       0x00
#define DRV_LIBRARY_A           0x01
#define DRV_LIBRARY_B           0x02
#define DRV_LIBRARY_C           0x03
#define DRV_LIBRARY_D           0x04
#define DRV_LIBRARY_E           0x05
#define DRV_LIBRARY_LRA         0x06

// ── Named effects for smartwatch ─────────────────────────────
// These are the effect numbers that feel best on a watch

#define HAPTIC_STRONG_CLICK     1
#define HAPTIC_SHARP_CLICK      4
#define HAPTIC_SOFT_BUMP        7
#define HAPTIC_DOUBLE_CLICK     10
#define HAPTIC_TRIPLE_CLICK     14
#define HAPTIC_BUZZ             47
#define HAPTIC_ALERT_BUZZ       48
#define HAPTIC_SOS_PULSE        52
#define HAPTIC_SUCCESS          56
#define HAPTIC_DISMISS          58
#define HAPTIC_STEP_MILESTONE   84
#define HAPTIC_WARNING          97
#define HAPTIC_SUBTLE_HUM       118

// ── Preset patterns for watch events ─────────────────────────
typedef enum {
    HAPTIC_PATTERN_NOTIFICATION = 0,
    HAPTIC_PATTERN_CALL,
    HAPTIC_PATTERN_ALARM,
    HAPTIC_PATTERN_SOS,
    HAPTIC_PATTERN_STEP_GOAL,
    HAPTIC_PATTERN_HEART_RATE_DONE,
    HAPTIC_PATTERN_BUTTON_CLICK,
    HAPTIC_PATTERN_SWIPE,
    HAPTIC_PATTERN_SUCCESS,
    HAPTIC_PATTERN_WARNING,
    HAPTIC_PATTERN_MAX
} haptic_pattern_t;

// ── Function declarations ─────────────────────────────────────
esp_err_t  drv2605l_init(void);
esp_err_t  drv2605l_set_waveform(uint8_t slot, uint8_t effect);
esp_err_t  drv2605l_go(void);
esp_err_t  drv2605l_stop(void);
esp_err_t  drv2605l_play_effect(uint8_t effect);
esp_err_t  drv2605l_play_pattern(haptic_pattern_t pattern);
esp_err_t  drv2605l_standby(bool enable);
esp_err_t  drv2605l_run_calibration(void);

#endif