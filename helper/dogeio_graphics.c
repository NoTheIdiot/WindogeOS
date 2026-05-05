#include <stdint.h>
#include <stddef.h>
#include "dogeio_graphics.h"
#include "dogeio.h"
#include "vbe.h"
#include "font.h"
#include "ports.h"
#include "../kernel/multiboot.h"

extern char scan_to_ascii[];
extern char scan_to_ascii_shift[];

// variables
static uint16_t gfx_cursor_x = 0;
static uint16_t gfx_cursor_y = 0;
static uint16_t gfx_char_width = 8;
static uint16_t gfx_char_height = 12;
static uint16_t gfx_max_cols = 0;
static uint16_t gfx_max_rows = 0;

void dogeio_init_graphics_from_multiboot(multiboot_info_t* mbi) {
    vbe_init_from_multiboot(mbi);
    
    if (!vbe_initialized) {
        return;
    }
    
    gfx_max_cols = vbe_width / gfx_char_width;
    gfx_max_rows = vbe_height / gfx_char_height;
    
    dogeio_clear_screen_graphics();
}


void dogeio_init_graphics() {

    gfx_max_cols = vbe_width / gfx_char_width;
    gfx_max_rows = vbe_height / gfx_char_height;
    
    // Clear screen
    dogeio_clear_screen_graphics();
}

void dogeio_clear_screen_graphics() {
    uint32_t bg_color = vbe_make_color(0x1a, 0x1a, 0x1a);
    for (uint16_t y = 0; y < vbe_height; y++) {
        for (uint16_t x = 0; x < vbe_width; x++) {
            vbe_putpixel(x, y, bg_color);
        }
    }

    gfx_cursor_x = 0;
    gfx_cursor_y = 0;
}

static void dogeio_scroll_graphics() {
    uint8_t* frame_buffer = (uint8_t*)vbe_frame_buffer;
    size_t row_bytes = (size_t)vbe_pitch * gfx_char_height;
    size_t total_bytes = (size_t)vbe_pitch * vbe_height;
    size_t move_bytes = total_bytes - row_bytes;

    for (size_t i = move_bytes; i > 0; i--) {
        frame_buffer[i - 1] = frame_buffer[i - 1 + row_bytes];
    }

    uint32_t bg_color = vbe_make_color(0x1a, 0x1a, 0x1a);
    for (uint16_t y = vbe_height - gfx_char_height; y < vbe_height; y++) {
        for (uint16_t x = 0; x < vbe_width; x++) {
            vbe_putpixel(x, y, bg_color);
        }
    }

    gfx_cursor_x = 0;
    gfx_cursor_y = gfx_max_rows - 1;
}

// Put a single character
void dogeio_putchar_graphics(char c) {
    if (c == '\n') {
        gfx_cursor_x = 0;
        gfx_cursor_y++;
    } else if (c == '\r') {
        gfx_cursor_x = 0;
    } else if (c == '\t') {
        gfx_cursor_x += 4;
    } else {
        uint16_t pixel_x = gfx_cursor_x * gfx_char_width;
        uint16_t pixel_y = gfx_cursor_y * gfx_char_height;
        
        uint32_t fg_color = vbe_make_color(0xe0, 0xe0, 0xe0);
        uint32_t bg_color = vbe_make_color(0x1a, 0x1a, 0x1a);
        
        font_draw_char(pixel_x, pixel_y, c, fg_color, bg_color);
        gfx_cursor_x++;
    }
    
    if (gfx_cursor_x >= gfx_max_cols) {
        gfx_cursor_x = 0;
        gfx_cursor_y++;
    }

    if (gfx_cursor_y >= gfx_max_rows) {
        dogeio_scroll_graphics();
        gfx_cursor_y = gfx_max_rows - 1;
    }
}

// Print a string
void dogeio_print_graphics(const char* string) {
    size_t i = 0;
    for (i = 0; string[i] != '\0'; i++) {
        dogeio_putchar_graphics(string[i]);
    }
}

// Print a string with newline
void dogeio_println_graphics(const char* string) {
    dogeio_print_graphics(string);
    dogeio_putchar_graphics('\n');
}

// Input for graphics mode
void dogeio_input_graphics(char* buffer, int max_len, uint8_t color) {
    (void)color;
    int i = 0;
    int shift = 0;
    uint32_t fg_color = vbe_make_color(0xe0, 0xe0, 0xe0);
    uint32_t bg_color = vbe_make_color(0x1a, 0x1a, 0x1a);

    while (i < max_len - 1) {
        while (!(ports_inb(0x64) & 0x01));
        uint8_t sc = ports_inb(0x60);

        if (sc == 0x2A || sc == 0x36) { shift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift = 0; continue; }
        if (sc & 0x80) continue;
        if (sc == 0x01) break;

        char c = (shift) ? scan_to_ascii_shift[sc] : scan_to_ascii[sc];
        if (!c) continue;

        if (c == '\n') {
            buffer[i] = '\0';
            gfx_cursor_x = 0;
            gfx_cursor_y++;
            if (gfx_cursor_y >= gfx_max_rows) {
                dogeio_scroll_graphics();
                gfx_cursor_y = gfx_max_rows - 1;
            }
            return;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                buffer[i] = '\0';
                if (gfx_cursor_x > 0) {
                    gfx_cursor_x--;
                } else if (gfx_cursor_y > 0) {
                    gfx_cursor_y--;
                    gfx_cursor_x = gfx_max_cols - 1;
                }
                uint16_t pixel_x = gfx_cursor_x * gfx_char_width;
                uint16_t pixel_y = gfx_cursor_y * gfx_char_height;
                font_draw_char(pixel_x, pixel_y, ' ', bg_color, bg_color);
            }
        } else {
            buffer[i++] = c;
            uint16_t pixel_x = gfx_cursor_x * gfx_char_width;
            uint16_t pixel_y = gfx_cursor_y * gfx_char_height;
            font_draw_char(pixel_x, pixel_y, c, fg_color, bg_color);
            gfx_cursor_x++;

            if (gfx_cursor_x >= gfx_max_cols) {
                gfx_cursor_x = 0;
                gfx_cursor_y++;
            }
            if (gfx_cursor_y >= gfx_max_rows) {
                dogeio_scroll_graphics();
                gfx_cursor_y = gfx_max_rows - 1;
            }
        }
    }
    buffer[i] = '\0';
}

