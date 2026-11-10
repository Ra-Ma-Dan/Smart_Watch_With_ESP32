#include "my_spi.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MY_SPI";
static bool bus_initialized = false;

// ── Init ──────────────────────────────────────────────────────

esp_err_t spi_driver_init(spi_device_handle_t *handle, gpio_num_t cs_pin, int clock_hz) {
    if (!bus_initialized) {
        spi_bus_config_t bus_cfg = {
            .mosi_io_num     = SPI_MOSI_PIN,
            .miso_io_num     = SPI_MISO_PIN,
            .sclk_io_num     = SPI_CLK_PIN,
            .quadwp_io_num   = -1,
            .quadhd_io_num   = -1,
            .max_transfer_sz = 32768   // 32KB max per transfer
        };

        // ESP-IDF v6.0 uses SPI_DMA_CH_AUTO for streamlined internal hardware channel resolution
        esp_err_t ret = spi_bus_initialize(SPI_HOST_BUS, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI bus init failed");
            return ret;
        }

        bus_initialized = true;
        ESP_LOGI(TAG, "SPI bus initialized successfully");
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = clock_hz,
        .mode           = 0,           // SPI mode 0, CPOL=0 CPHA=0
        .spics_io_num   = cs_pin,
        .queue_size     = 7,           //
        .flags          = SPI_DEVICE_NO_DUMMY, // Modern optimization flags for screens
    };

    esp_err_t ret = spi_bus_add_device(SPI_HOST_BUS, &dev_cfg, handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device configuration assignment failed");
        return ret;
    }

    return ESP_OK;
}

// ── Send single byte ──────────────────────────────────────────

esp_err_t spi_driver_send_byte(spi_device_handle_t handle, uint8_t byte) {
    spi_transaction_t t = {
        .flags     = SPI_TRANS_USE_TXDATA, // CRITICAL FOR ESP-IDF v6: Tells hardware to use internal buffer array rather than raw stack pointers
        .length    = 8,                    // 8 bits
        .tx_data   = { byte },             // Safely copies byte into internal transaction memory
        .rx_buffer = NULL                  //
    };

    return spi_device_transmit(handle, &t);
}

// ── Send buffer ───────────────────────────────────────────────

esp_err_t spi_driver_send_buffer(spi_device_handle_t handle, const uint8_t *data, size_t len) {
    if (len == 0) return ESP_OK; //

    spi_transaction_t t = {
        .length    = len * 8,    // Length in bits
        .tx_buffer = data,       // For large arrays (like screen framebuffers), ensure allocation via heap_caps_malloc(..., MALLOC_CAP_DMA)
        .rx_buffer = NULL        //
    };

    return spi_device_transmit(handle, &t);
}
