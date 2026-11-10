#ifndef GC9A01_H
#define GC9A01_H

#include "my_spi.h"
#include "my_gpio.h"
#include <stdint.h>

// ── Screen dimensions ─────────────────────────────────────────
#define GC9A01_WIDTH    240
#define GC9A01_HEIGHT   240

// ── GC9A01 commands ───────────────────────────────────────────
#define GC9A01_CMD_SWRESET   0x01   // software reset
#define GC9A01_CMD_SLPOUT    0x11   // sleep out
#define GC9A01_CMD_INVON     0x21   // invert on
#define GC9A01_CMD_DISPON    0x29   // display on
#define GC9A01_CMD_CASET     0x2A   // column address set
#define GC9A01_CMD_RASET     0x2B   // row address set
#define GC9A01_CMD_RAMWR     0x2C   // write to RAM
#define GC9A01_CMD_COLMOD    0x3A   // color format
#define GC9A01_CMD_MADCTL    0x36   // memory access control

// ── Color format ──────────────────────────────────────────────
#define GC9A01_COLOR_MODE_16BIT  0x55   // RGB565

// ── Color helpers ─────────────────────────────────────────────
#define RGB565(r, g, b)   ((uint16_t)(((r & 0x1F) << 11) | \
                                       ((g & 0x3F) << 5)  | \
                                        (b & 0x1F)))

#define COLOR_BLACK     RGB565(0,   0,   0)
#define COLOR_WHITE     RGB565(31,  63,  31)
#define COLOR_RED       RGB565(31,  0,   0)
#define COLOR_GREEN     RGB565(0,   63,  0)
#define COLOR_BLUE      RGB565(0,   0,   31)
#define COLOR_YELLOW    RGB565(31,  63,  0)
#define COLOR_CYAN      RGB565(0,   63,  31)
#define COLOR_MAGENTA   RGB565(31,  0,   31)
#define COLOR_ORANGE    RGB565(31,  25,  0)

// ── Function declarations ─────────────────────────────────────
esp_err_t gc9a01_init(void);

void gc9a01_fill_screen(uint16_t color);
void gc9a01_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void gc9a01_fill_rect(uint16_t x,     uint16_t y,
                       uint16_t width, uint16_t height,
                       uint16_t color);
void gc9a01_draw_circle(uint16_t cx,     uint16_t cy,
                         uint16_t radius, uint16_t color);
void gc9a01_draw_string(uint16_t    x,     uint16_t y,
                         const char *str,
                         uint16_t    color, uint16_t bg_color,
                         uint8_t     size);
                         
// Add to gc9a01.h
void gc9a01_set_window(uint16_t x, uint16_t y,
                        uint16_t w, uint16_t h);
void gc9a01_push_pixels(const uint8_t *data, size_t len);

#endif