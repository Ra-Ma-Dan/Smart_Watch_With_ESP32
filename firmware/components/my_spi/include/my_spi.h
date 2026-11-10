#ifndef MY_SPI_H
#define MY_SPI_H

#include "driver/spi_master.h"
#include "hal/gpio_types.h"
#include "esp_err.h"
#include <stdint.h>

// ── SPI bus configuration ─────────────────────────────────────
#define SPI_HOST_BUS     SPI2_HOST
#define SPI_CLOCK_HZ     80000000    // 80 MHz
#define SPI_MOSI_PIN     GPIO_NUM_11 // Replaced placeholder macros with explicit modern typings
#define SPI_CLK_PIN      GPIO_NUM_12
#define SPI_MISO_PIN     GPIO_NUM_NC // Display is write-only

// ── Function declarations ─────────────────────────────────────
esp_err_t spi_driver_init(spi_device_handle_t *handle, gpio_num_t cs_pin, int clock_hz);
esp_err_t spi_driver_send_byte(spi_device_handle_t handle, uint8_t byte);
esp_err_t spi_driver_send_buffer(spi_device_handle_t handle, const uint8_t *data, size_t len);

#endif // MY_SPI_H
