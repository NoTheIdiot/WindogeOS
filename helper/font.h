#ifndef FONT_H
#define FONT_H

#include <stdint.h>

// 8x8 bitmap font (ASCII 32-126)
extern const uint8_t font_data[95][8];

// Font rendering functions
void font_draw_char(uint16_t x, uint16_t y, char c, uint32_t fg_color, uint32_t bg_color);
void font_draw_string(uint16_t x, uint16_t y, const char* string, uint32_t fg_color, uint32_t bg_color);

#endif
