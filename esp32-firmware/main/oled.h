#pragma once

#include "driver/i2c_master.h"

#define OLED_I2C_ADDR      0x3C
#define OLED_WIDTH         128
#define OLED_HEIGHT        64
#define OLED_PAGES         (OLED_HEIGHT / 8)

// 6x8 font: 21 cols x 8 rows on 128x64 display
#define OLED_COLS          21
#define OLED_ROWS          8

void oled_init(i2c_master_bus_handle_t bus);
void oled_clear(void);
void oled_flush(void);
void oled_set_cursor(uint8_t col, uint8_t row);
void oled_write_char(char c);
void oled_write_str(const char *s);
void oled_write_line(uint8_t row, const char *s);

// Draw a filled rectangle (for status bars, etc.)
void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);
