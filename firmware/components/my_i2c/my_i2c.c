#include "my_i2c.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "MY_I2C";

// Static global bus handle
static i2c_master_bus_handle_t bus_handle = NULL;

// Raw 7-bit device addresses mapped from your legacy configuration
#define I2C_ADDR_VEML7700   0x10
#define I2C_ADDR_CST816S    0x15
#define I2C_ADDR_AXP2101    0x34
#define I2C_ADDR_MAX17048   0x36
#define I2C_ADDR_TMP117     0x48
#define I2C_ADDR_PCF85063A  0x51
#define I2C_ADDR_MAX30102   0x57
#define I2C_ADDR_DRV2605L   0x5A
#define I2C_ADDR_BMI270     0x69

// Define global handles declared in the header
i2c_master_dev_handle_t veml7700_handle  = NULL;
i2c_master_dev_handle_t cst816s_handle   = NULL;
i2c_master_dev_handle_t axp2101_handle   = NULL;
i2c_master_dev_handle_t max17048_handle  = NULL;
i2c_master_dev_handle_t tmp117_handle    = NULL;
i2c_master_dev_handle_t pcf85063a_handle = NULL;
i2c_master_dev_handle_t max30102_handle  = NULL;
i2c_master_dev_handle_t drv2605l_handle  = NULL;
i2c_master_dev_handle_t bmi270_handle    = NULL;

// Helper to bundle repetitive device installations
static esp_err_t add_i2c_device(uint16_t dev_addr, i2c_master_dev_handle_t *dev_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = I2C_CLOCK_HZ,
    };
    return i2c_master_bus_add_device(bus_handle, &dev_cfg, dev_handle);
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t i2c_driver_init(void) {
    // 1. Configure and initialize the main controller bus
    i2c_master_bus_config_t bus_config = {
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .i2c_port            = I2C_PORT,
        .scl_io_num          = I2C_SCL_PIN,
        .sda_io_num          = I2C_SDA_PIN,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true, // Activates internal pull-ups safely
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C master bus");
        return ret;
    }

    // 2. Link each individual device handle to the managed bus
    // The driver sets up implicit mutual exclusion locks globally for these devices
    if (add_i2c_device(I2C_ADDR_VEML7700,  &veml7700_handle)  != ESP_OK ||
        add_i2c_device(I2C_ADDR_CST816S,   &cst816s_handle)   != ESP_OK ||
        add_i2c_device(I2C_ADDR_AXP2101,   &axp2101_handle)   != ESP_OK ||
        add_i2c_device(I2C_ADDR_MAX17048,  &max17048_handle)  != ESP_OK ||
        add_i2c_device(I2C_ADDR_TMP117,    &tmp117_handle)    != ESP_OK ||
        add_i2c_device(I2C_ADDR_PCF85063A, &pcf85063a_handle) != ESP_OK ||
        add_i2c_device(I2C_ADDR_MAX30102,  &max30102_handle)  != ESP_OK ||
        add_i2c_device(I2C_ADDR_DRV2605L,  &drv2605l_handle)  != ESP_OK ||
        add_i2c_device(I2C_ADDR_BMI270,    &bmi270_handle)    != ESP_OK) {
        
        ESP_LOGE(TAG, "Failed to register one or more I2C devices to the bus");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2C Bus and 9 device hooks initialized at %d Hz", I2C_CLOCK_HZ);
    return ESP_OK;
}

// ── Write ─────────────────────────────────────────────────────

esp_err_t i2c_driver_write(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, const uint8_t *data, size_t len) {
    if (dev_handle == NULL) return ESP_ERR_INVALID_ARG;

    size_t write_len = len + 1;
    uint8_t *write_buf = malloc(write_len);
    if (write_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Set register address at position 0, then copy following bytes
    write_buf[0] = reg_addr;
    if (len > 0 && data != NULL) {
        memcpy(&write_buf[1], data, len);
    }

    // i2c_master_transmit performs atomic bus locking and treats timeout as milliseconds directly
    esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, write_len, I2C_TIMEOUT_MS);
    free(write_buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write transaction failed on target handle reg: 0x%02X (Error: %s)", reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

// ── Read ──────────────────────────────────────────────────────

esp_err_t i2c_driver_read(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *buffer, size_t len) {
    if (dev_handle == NULL) return ESP_ERR_INVALID_ARG;

    // Streamlined combined transaction: Writes register address, executes a repeated start, and reads into the buffer
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, &reg_addr, 1, buffer, len, I2C_TIMEOUT_MS);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read transaction failed on target handle reg: 0x%02X (Error: %s)", reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

// ── Single byte helpers ───────────────────────────────────────

esp_err_t i2c_driver_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t value) {
    return i2c_driver_write(dev_handle, reg_addr, &value, 1);
}

esp_err_t i2c_driver_read_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *value) {
    return i2c_driver_read(dev_handle, reg_addr, value, 1);
}

// ── Bus scan ──────────────────────────────────────────────────

esp_err_t i2c_driver_scan(void) {
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Cannot scan, master bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Scanning active I2C bus...");

    int found = 0;
    for (uint16_t addr = 1; addr < 127; addr++) {
        // Probe addresses safely using the internal testing utility without initializing persistent devices
        if (i2c_master_probe(bus_handle, addr, I2C_TIMEOUT_MS) == ESP_OK) {
            ESP_LOGI(TAG, "  Found device at 0x%02X", addr);
            found++;
        }
    }

    ESP_LOGI(TAG, "Scan complete. %d device(s) found", found);
    return ESP_OK;
}
