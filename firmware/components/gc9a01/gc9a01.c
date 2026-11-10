#include "gc9a01.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>


static const char *TAG        = "GC9A01";
static spi_device_handle_t gh = NULL;   // display SPI handle

// ── Basic 5x7 font ────────────────────────────────────────────
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00},  // space (0x20)
    {0x00,0x00,0x5F,0x00,0x00},  // !
    {0x00,0x07,0x00,0x07,0x00},  // "
    {0x14,0x7F,0x14,0x7F,0x14},  // #
    {0x24,0x2A,0x7F,0x2A,0x12},  // $
    {0x23,0x13,0x08,0x64,0x62},  // %
    {0x36,0x49,0x55,0x22,0x50},  // &
    {0x00,0x05,0x03,0x00,0x00},  // '
    {0x00,0x1C,0x22,0x41,0x00},  // (
    {0x00,0x41,0x22,0x1C,0x00},  // )
    {0x08,0x2A,0x1C,0x2A,0x08},  // *
    {0x08,0x08,0x3E,0x08,0x08},  // +
    {0x00,0x50,0x30,0x00,0x00},  // ,
    {0x08,0x08,0x08,0x08,0x08},  // -
    {0x00,0x60,0x60,0x00,0x00},  // .
    {0x20,0x10,0x08,0x04,0x02},  // /
    {0x3E,0x51,0x49,0x45,0x3E},  // 0
    {0x00,0x42,0x7F,0x40,0x00},  // 1
    {0x42,0x61,0x51,0x49,0x46},  // 2
    {0x21,0x41,0x45,0x4B,0x31},  // 3
    {0x18,0x14,0x12,0x7F,0x10},  // 4
    {0x27,0x45,0x45,0x45,0x39},  // 5
    {0x3C,0x4A,0x49,0x49,0x30},  // 6
    {0x01,0x71,0x09,0x05,0x03},  // 7
    {0x36,0x49,0x49,0x49,0x36},  // 8
    {0x06,0x49,0x49,0x29,0x1E},  // 9
};

// ── Low level send helpers ────────────────────────────────────

static void send_cmd(uint8_t cmd) {
    gpio_driver_set_low(PIN_DISPLAY_DC);    // DC LOW = command
    spi_driver_send_byte(gh, cmd);
}

static void send_data_byte(uint8_t data) {
    gpio_driver_set_high(PIN_DISPLAY_DC);   // DC HIGH = data
    spi_driver_send_byte(gh, data);
}

static void send_data_buffer(const uint8_t *buf, size_t len) {
    gpio_driver_set_high(PIN_DISPLAY_DC);
    spi_driver_send_buffer(gh, buf, len);
}

// ── Set drawing window ────────────────────────────────────────

static void set_window(uint16_t x, uint16_t y,
                        uint16_t w, uint16_t h) {
    uint16_t x2 = x + w - 1;
    uint16_t y2 = y + h - 1;

    send_cmd(GC9A01_CMD_CASET);
    send_data_byte(x  >> 8); send_data_byte(x  & 0xFF);
    send_data_byte(x2 >> 8); send_data_byte(x2 & 0xFF);

    send_cmd(GC9A01_CMD_RASET);
    send_data_byte(y  >> 8); send_data_byte(y  & 0xFF);
    send_data_byte(y2 >> 8); send_data_byte(y2 & 0xFF);

    send_cmd(GC9A01_CMD_RAMWR);   // ready to receive pixels
}

// ── Init sequence ─────────────────────────────────────────────

esp_err_t gc9a01_init(void) {

    // Add display to SPI bus
    esp_err_t ret = spi_driver_init(&gh,
                                     PIN_DISPLAY_CS,
                                     SPI_CLOCK_HZ);
    if (ret != ESP_OK) return ret;

    // Hardware reset
    gpio_driver_set_low(PIN_DISPLAY_RST);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_driver_set_high(PIN_DISPLAY_RST);
    vTaskDelay(pdMS_TO_TICKS(120));

    // GC9A01 initialization sequence from datasheet
    send_cmd(0xEF);

    send_cmd(0xEB);
    send_data_byte(0x14);

    send_cmd(0xFE);
    send_cmd(0xEF);

    send_cmd(0xEB);
    send_data_byte(0x14);

    send_cmd(0x84);
    send_data_byte(0x40);

    send_cmd(0x85);
    send_data_byte(0xFF);

    send_cmd(0x86);
    send_data_byte(0xFF);

    send_cmd(0x87);
    send_data_byte(0xFF);

    send_cmd(0x88);
    send_data_byte(0x0A);

    send_cmd(0x89);
    send_data_byte(0x21);

    send_cmd(0x8A);
    send_data_byte(0x00);

    send_cmd(0x8B);
    send_data_byte(0x80);

    send_cmd(0x8C);
    send_data_byte(0x01);

    send_cmd(0x8D);
    send_data_byte(0x01);

    send_cmd(0x8E);
    send_data_byte(0xFF);

    send_cmd(0x8F);
    send_data_byte(0xFF);

    send_cmd(GC9A01_CMD_MADCTL);
    send_data_byte(0x48);                    // orientation

    send_cmd(GC9A01_CMD_COLMOD);
    send_data_byte(GC9A01_COLOR_MODE_16BIT); // RGB565

    send_cmd(GC9A01_CMD_INVON);   // inversion on for correct colors
    send_cmd(GC9A01_CMD_SLPOUT);  // wake from sleep

    vTaskDelay(pdMS_TO_TICKS(120));

    send_cmd(GC9A01_CMD_DISPON);  // display on

    ESP_LOGI(TAG, "GC9A01 initialized — 240x240 round display ready");
    return ESP_OK;
}

// ── Drawing functions ─────────────────────────────────────────

void gc9a01_fill_screen(uint16_t color) {
    gc9a01_fill_rect(0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, color);
}

void gc9a01_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= GC9A01_WIDTH || y >= GC9A01_HEIGHT) return;
    set_window(x, y, 1, 1);
    send_data_byte(color >> 8);
    send_data_byte(color & 0xFF);
}

void gc9a01_fill_rect(uint16_t x,     uint16_t y,
                       uint16_t width, uint16_t height,
                       uint16_t color) {

    if (x >= GC9A01_WIDTH || y >= GC9A01_HEIGHT) return;

    // Clamp to screen bounds
    if (x + width  > GC9A01_WIDTH)  width  = GC9A01_WIDTH  - x;
    if (y + height > GC9A01_HEIGHT) height = GC9A01_HEIGHT - y;

    set_window(x, y, width, height);

    // Build a line buffer and send row by row
    uint32_t total  = width * height;
    uint8_t  hi     = color >> 8;
    uint8_t  lo     = color & 0xFF;

    // Use a 64 pixel buffer to avoid large stack allocation
    uint8_t  buf[128];
    for (int i = 0; i < 128; i += 2) {
        buf[i]   = hi;
        buf[i+1] = lo;
    }

    uint32_t remaining = total;
    while (remaining > 0) {
        uint32_t chunk = remaining > 64 ? 64 : remaining;
        send_data_buffer(buf, chunk * 2);
        remaining -= chunk;
    }
}

// ── Circle using Bresenham algorithm ─────────────────────────

void gc9a01_draw_circle(uint16_t cx,     uint16_t cy,
                         uint16_t radius, uint16_t color) {
    int16_t x   = radius;
    int16_t y   = 0;
    int16_t err = 0;

    while (x >= y) {
        gc9a01_draw_pixel(cx + x, cy + y, color);
        gc9a01_draw_pixel(cx + y, cy + x, color);
        gc9a01_draw_pixel(cx - y, cy + x, color);
        gc9a01_draw_pixel(cx - x, cy + y, color);
        gc9a01_draw_pixel(cx - x, cy - y, color);
        gc9a01_draw_pixel(cx - y, cy - x, color);
        gc9a01_draw_pixel(cx + y, cy - x, color);
        gc9a01_draw_pixel(cx + x, cy - y, color);

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

// ── Text ──────────────────────────────────────────────────────

void gc9a01_draw_string(uint16_t    x,        uint16_t y,
                         const char *str,
                         uint16_t    color,    uint16_t bg_color,
                         uint8_t     size) {

    uint16_t cursor_x = x;

    while (*str) {
        char c = *str++;

        // Only handle printable ASCII
        if (c < 0x20 || c > 0x39 + 25) {
            cursor_x += 6 * size;
            continue;
        }

        const uint8_t *glyph = font5x7[c - 0x20];

        for (uint8_t col = 0; col < 5; col++) {
            uint8_t col_data = glyph[col];

            for (uint8_t row = 0; row < 7; row++) {
                uint16_t px = cursor_x + col * size;
                uint16_t py = y        + row * size;

                uint16_t pixel_color = (col_data & (1 << row))
                                        ? color
                                        : bg_color;

                if (size == 1) {
                    gc9a01_draw_pixel(px, py, pixel_color);
                } else {
                    gc9a01_fill_rect(px, py,
                                     size, size,
                                     pixel_color);
                }
            }
        }

        cursor_x += 6 * size;   // 5 pixels wide + 1 gap
    }
}

// Add to gc9a01.c
void gc9a01_set_window(uint16_t x, uint16_t y,
                        uint16_t w, uint16_t h) {
    set_window(x, y, w, h);   // calls the existing static function
}

void gc9a01_push_pixels(const uint8_t *data, size_t len) {
    send_data_buffer(data, len);
}