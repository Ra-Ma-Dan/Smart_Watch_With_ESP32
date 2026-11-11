#include "MAX30102.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

// ── Sample buffers ────────────────────────────────────────────
static uint32_t red_buf[MAX30102_BUFFER_SIZE];
static uint32_t ir_buf[MAX30102_BUFFER_SIZE];

// ── Reset FIFO pointers ───────────────────────────────────────

static esp_err_t fifo_reset(void) {
    esp_err_t ret;
    ret = i2c_driver_write_byte(MAX30102_ADDR,
                                 MAX_FIFO_WR_PTR, 0x00);
    if (ret != ESP_OK) return ret;
    ret = i2c_driver_write_byte(MAX30102_ADDR,
                                 MAX_FIFO_OVF_CTR, 0x00);
    if (ret != ESP_OK) return ret;
    return i2c_driver_write_byte(MAX30102_ADDR,
                                  MAX_FIFO_RD_PTR, 0x00);
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t max30102_init(void) {

    // Confirm part ID
    uint8_t id = 0;
    esp_err_t ret = i2c_driver_read_byte(MAX30102_ADDR,
                                          MAX_PART_ID, &id);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_HEART, "MAX30102 not found on I2C bus");
        return ret;
    }

    if (id != MAX30102_PART_ID) {
        LOG_ERROR(TAG_HEART,
                  "Wrong part ID 0x%02X expected 0x%02X",
                  id, MAX30102_PART_ID);
        return ESP_ERR_NOT_FOUND;
    }

    LOG_INFO(TAG_HEART, "MAX30102 found — part ID 0x%02X", id);

    // Software reset
    i2c_driver_write_byte(MAX30102_ADDR,
                           MAX_MODE_CFG, MAX_MODE_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));

    // FIFO config — 4 sample averaging, rollover enabled
    i2c_driver_write_byte(MAX30102_ADDR,
                           MAX_FIFO_CFG,
                           MAX_FIFO_AVG_4 | MAX_FIFO_ROLLOVER_EN);

    // SpO2 mode — red + IR
    i2c_driver_write_byte(MAX30102_ADDR,
                           MAX_MODE_CFG, MAX_MODE_SPO2);

    // SpO2 config
    // ADC range 16384, 100 samples/sec, 411µs pulse width (18 bit)
    i2c_driver_write_byte(MAX30102_ADDR,
                           MAX_SPO2_CFG,
                           MAX_ADC_RANGE_16384 |
                           MAX_SAMPLE_RATE_100 |
                           MAX_PULSE_WIDTH_411);

    // LED amplitudes
    // 0x24 = ~7.2mA for red LED
    // 0x24 = ~7.2mA for IR LED
    // Higher = more light = better signal but more power
    i2c_driver_write_byte(MAX30102_ADDR, MAX_LED1_AMP, 0x24);
    i2c_driver_write_byte(MAX30102_ADDR, MAX_LED2_AMP, 0x24);

    // Enable data ready interrupt
    i2c_driver_write_byte(MAX30102_ADDR,
                           MAX_INT_EN1, 0xC0);

    // Reset FIFO
    fifo_reset();

    LOG_INFO(TAG_HEART, "MAX30102 initialized");
    return ESP_OK;
}

// ── Check if finger is on sensor ─────────────────────────────
// IR value above 50000 means something is covering the sensor

bool max30102_finger_detected(void) {
    uint32_t dummy_red[4];
    uint32_t dummy_ir[4];
    max30102_read_samples(dummy_red, dummy_ir, 4);

    // Finger present if IR reading is strong enough
    return dummy_ir[0] > 50000;
}

// ── Read samples from FIFO ────────────────────────────────────

esp_err_t max30102_read_samples(uint32_t *red,
                                 uint32_t *ir,
                                 uint8_t   count) {
    for (uint8_t i = 0; i < count; i++) {
        uint8_t buf[6] = {0};

        esp_err_t ret = i2c_driver_read(MAX30102_ADDR,
                                         MAX_FIFO_DATA,
                                         buf, 6);
        if (ret != ESP_OK) return ret;

        // Each sample is 3 bytes, upper 18 bits valid
        red[i] = ((uint32_t)(buf[0] & 0x03) << 16) |
                  ((uint32_t) buf[1]          << 8)  |
                   (uint32_t) buf[2];

        ir[i]  = ((uint32_t)(buf[3] & 0x03) << 16) |
                  ((uint32_t) buf[4]          << 8)  |
                   (uint32_t) buf[5];
    }
    return ESP_OK;
}

// ── DC removal using mean subtraction ────────────────────────

static void remove_dc(uint32_t *buf, float *out, uint8_t len) {
    // Calculate mean
    float sum = 0;
    for (uint8_t i = 0; i < len; i++) sum += buf[i];
    float mean = sum / len;

    // Subtract mean from each sample
    for (uint8_t i = 0; i < len; i++) {
        out[i] = (float)buf[i] - mean;
    }
}

// ── Find peaks in AC signal ───────────────────────────────────

static uint8_t find_peaks(float *signal,
                            uint8_t len,
                            uint8_t *peak_locs) {
    uint8_t count = 0;
    for (uint8_t i = 1; i < len - 1; i++) {
        if (signal[i] > signal[i-1] &&
            signal[i] > signal[i+1] &&
            signal[i] > 200.0f) {          // minimum peak height
            peak_locs[count++] = i;
            if (count >= 20) break;        // cap at 20 peaks
        }
    }
    return count;
}

// ── Main measurement function ─────────────────────────────────

esp_err_t max30102_measure(max30102_result_t *result) {

    memset(result, 0, sizeof(max30102_result_t));

    // Check finger is present first
    result->finger_detected = max30102_finger_detected();
    if (!result->finger_detected) {
        LOG_WARN(TAG_HEART, "No finger detected");
        return ESP_OK;
    }

    // Collect 100 samples at 100 samples/sec = ~1 second of data
    LOG_INFO(TAG_HEART, "Collecting samples...");
    fifo_reset();

    // Wait for FIFO to fill with 100 samples
    // At 100sps with 4 sample averaging = 25 actual writes/sec
    // 100 samples takes ~4 seconds
    vTaskDelay(pdMS_TO_TICKS(4100));

    esp_err_t ret = max30102_read_samples(red_buf,
                                           ir_buf,
                                           MAX30102_BUFFER_SIZE);
    if (ret != ESP_OK) return ret;

    // ── Heart rate calculation ────────────────────────────────

    float ir_ac[MAX30102_BUFFER_SIZE];
    remove_dc(ir_buf, ir_ac, MAX30102_BUFFER_SIZE);

    uint8_t peak_locs[20];
    uint8_t peak_count = find_peaks(ir_ac,
                                     MAX30102_BUFFER_SIZE,
                                     peak_locs);

    if (peak_count >= 2) {
        // Average distance between peaks in samples
        float avg_distance = 0;
        for (uint8_t i = 1; i < peak_count; i++) {
            avg_distance += peak_locs[i] - peak_locs[i-1];
        }
        avg_distance /= (peak_count - 1);

        // Samples per second = 100 / averaging factor = 25
        float samples_per_sec = 25.0f;
        float bpm = (samples_per_sec / avg_distance) * 60.0f;

        // Sanity check — human heart rate is 40-220 BPM
        if (bpm >= 40.0f && bpm <= 220.0f) {
            result->heart_rate = (uint8_t)bpm;
            result->hr_valid   = true;
        }
    }

    // ── SpO2 calculation ──────────────────────────────────────

    // Find AC component — peak to peak of each signal
    uint32_t red_max = 0, red_min = UINT32_MAX;
    uint32_t ir_max  = 0, ir_min  = UINT32_MAX;

    for (uint8_t i = 0; i < MAX30102_BUFFER_SIZE; i++) {
        if (red_buf[i] > red_max) red_max = red_buf[i];
        if (red_buf[i] < red_min) red_min = red_buf[i];
        if (ir_buf[i]  > ir_max)  ir_max  = ir_buf[i];
        if (ir_buf[i]  < ir_min)  ir_min  = ir_buf[i];
    }

    float red_ac = red_max - red_min;
    float ir_ac2 = ir_max  - ir_min;

    // DC components — mean values
    float red_dc = 0, ir_dc = 0;
    for (uint8_t i = 0; i < MAX30102_BUFFER_SIZE; i++) {
        red_dc += red_buf[i];
        ir_dc  += ir_buf[i];
    }
    red_dc /= MAX30102_BUFFER_SIZE;
    ir_dc  /= MAX30102_BUFFER_SIZE;

    // R ratio
    if (ir_ac2 > 0 && ir_dc > 0 &&
        red_dc > 0 && red_ac > 0) {

        float R = (red_ac / red_dc) / (ir_ac2 / ir_dc);

        // SpO2 from empirical formula
        float spo2 = -45.060f * R * R
                    + 30.354f * R
                    + 94.845f;

        // Sanity check — valid SpO2 is 70-100%
        if (spo2 >= 70.0f && spo2 <= 100.0f) {
            result->spo2       = (uint8_t)spo2;
            result->spo2_valid = true;
        }
    }

    LOG_INFO(TAG_HEART,
             "HR: %d BPM (%s)   SpO2: %d%% (%s)",
             result->heart_rate,
             result->hr_valid   ? "valid" : "invalid",
             result->spo2,
             result->spo2_valid ? "valid" : "invalid");

    return ESP_OK;
}

// ── Shutdown ──────────────────────────────────────────────────

esp_err_t max30102_shutdown(void) {
    LOG_INFO(TAG_HEART, "MAX30102 shutdown");
    return i2c_driver_write_byte(MAX30102_ADDR,
                                  MAX_MODE_CFG,
                                  MAX_MODE_SHDN);
}

// ── Wake up ───────────────────────────────────────────────────

esp_err_t max30102_wakeup(void) {
    LOG_INFO(TAG_HEART, "MAX30102 wakeup");
    return i2c_driver_write_byte(MAX30102_ADDR,
                                  MAX_MODE_CFG,
                                  MAX_MODE_SPO2);
}