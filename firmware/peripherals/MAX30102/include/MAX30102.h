#ifndef MAX30102_H
#define MAX30102_H

#include "i2c_driver.h"
#include "uart_driver.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ── I2C address ───────────────────────────────────────────────
#define MAX30102_ADDR            I2C_ADDR_MAX30102   // 0x57

// ── Registers ─────────────────────────────────────────────────
#define MAX_INT_STAT1            0x00
#define MAX_INT_STAT2            0x01
#define MAX_INT_EN1              0x02
#define MAX_INT_EN2              0x03
#define MAX_FIFO_WR_PTR          0x04
#define MAX_FIFO_OVF_CTR         0x05
#define MAX_FIFO_RD_PTR          0x06
#define MAX_FIFO_DATA            0x07
#define MAX_FIFO_CFG             0x08
#define MAX_MODE_CFG             0x09
#define MAX_SPO2_CFG             0x0A
#define MAX_LED1_AMP             0x0C
#define MAX_LED2_AMP             0x0D
#define MAX_PART_ID              0xFF

// ── Part ID ───────────────────────────────────────────────────
#define MAX30102_PART_ID         0x15

// ── Mode config ───────────────────────────────────────────────
#define MAX_MODE_HR_ONLY         0x02   // red LED only
#define MAX_MODE_SPO2            0x03   // red + IR LEDs
#define MAX_MODE_RESET           0x40   // software reset
#define MAX_MODE_SHDN            0x80   // shutdown

// ── SpO2 config ───────────────────────────────────────────────
#define MAX_ADC_RANGE_4096       0x00
#define MAX_ADC_RANGE_8192       0x20
#define MAX_ADC_RANGE_16384      0x40
#define MAX_ADC_RANGE_32768      0x60
#define MAX_SAMPLE_RATE_100      0x00   // 100 samples per second
#define MAX_SAMPLE_RATE_200      0x04
#define MAX_SAMPLE_RATE_400      0x08
#define MAX_PULSE_WIDTH_69       0x00   // 15 bit ADC
#define MAX_PULSE_WIDTH_118      0x01   // 16 bit ADC
#define MAX_PULSE_WIDTH_215      0x02   // 17 bit ADC
#define MAX_PULSE_WIDTH_411      0x03   // 18 bit ADC

// ── FIFO config ───────────────────────────────────────────────
#define MAX_FIFO_AVG_1           0x00
#define MAX_FIFO_AVG_2           0x20
#define MAX_FIFO_AVG_4           0x40
#define MAX_FIFO_AVG_8           0x60
#define MAX_FIFO_ROLLOVER_EN     0x10

// ── Sample buffer ─────────────────────────────────────────────
#define MAX30102_BUFFER_SIZE     100

// ── Result struct ─────────────────────────────────────────────
typedef struct {
    uint8_t  heart_rate;      // beats per minute
    uint8_t  spo2;            // oxygen saturation 0-100%
    bool     hr_valid;        // true if heart rate is reliable
    bool     spo2_valid;      // true if SpO2 is reliable
    bool     finger_detected; // true if finger is on sensor
} max30102_result_t;

// ── Function declarations ─────────────────────────────────────
esp_err_t  max30102_init(void);
esp_err_t  max30102_read_samples(uint32_t *red_buf,
                                  uint32_t *ir_buf,
                                  uint8_t   count);
esp_err_t  max30102_measure(max30102_result_t *result);
bool       max30102_finger_detected(void);
esp_err_t  max30102_shutdown(void);
esp_err_t  max30102_wakeup(void);

#endif