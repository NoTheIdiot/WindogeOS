#include <stdint.h>
#include <boot/limine.h>
#include <dogeio.h>
#include <basicutil.h>
#include <boot/kernel.h>
#include <stddef.h>

extern volatile struct limine_framebuffer_request framebuffer_request;

const uint32_t ansi_colors[16] = {
    0x000000, 0xAA0000, 0x00AA00, 0xAA5500, 0x0000AA, 0xAA00AA, 0x00AAAA, 0xAAAAAA,
    0x555555, 0xFF5555, 0x55FF55, 0xFFFF55, 0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFFFF
};

const uint8_t* current_font = vga_text;

void dogeio_set_font(const uint8_t* font) {
    if (font != NULL) {
        current_font = font;
    }
}

const uint8_t* get_glyph_pointer(char c) {
    if ((uint8_t)c >= 128) {
        c = ' ';
    }

    uint32_t magic = *(const uint32_t*)current_font;

    if ((magic & 0xFFFF) == 0x0436) {
        uint8_t bytes_per_glyph = current_font[3]; 
        return &current_font[4 + ((uint8_t)c * bytes_per_glyph)];
    }
    else if (magic == 0x864ab572) {
        uint32_t header_size = *(const uint32_t*)&current_font[8];
        uint32_t bytes_per_glyph = *(const uint32_t*)&current_font[20];
        return &current_font[header_size + ((uint8_t)c * bytes_per_glyph)];
    }

    return current_font;
}

void dogeio_render_glyph(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    uint32_t* framebuffer_ptr = (uint32_t*)framebuffer->address;

    uint32_t pixel_x = x * 8;
    uint32_t pixel_y = y * 16;
    uint64_t u64_pitch = framebuffer->pitch / 4;

    if (pixel_x + 8 > framebuffer->width || pixel_y + 16 > framebuffer->height) {
        return;
    }

    const uint8_t* glyph = get_glyph_pointer(c);

    for (uint32_t g_row = 0; g_row < 16; g_row++) {
        uint8_t bits = glyph[g_row];
        size_t row_offset = (pixel_y + g_row) * u64_pitch + pixel_x;
        
        for (int bit = 0; bit < 8; bit++) {
            framebuffer_ptr[row_offset + (size_t)bit] = ((bits >> (7 - bit)) & 1) ? fg_color : bg_color;
        }
    }
}

void dogeio_putchar_raw_glyph(char c, uint32_t x, uint32_t y) {
    dogeio_render_glyph(c, x, y, dogeio_color, dogeio_background_color);
}

void dogeio_putchar_reverted_glyph(char c, uint32_t x, uint32_t y) {
    dogeio_render_glyph(c, x, y, dogeio_background_color, dogeio_color);
}

void dogeio_render_pixel_glyph(char c, uint32_t pixel_x, uint32_t pixel_y, uint32_t fg_color, uint32_t bg_color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    uint32_t* framebuffer_ptr = (uint32_t*)framebuffer->address;
    uint64_t u64_pitch = framebuffer->pitch / 4;

    if (pixel_x + 8 > framebuffer->width || pixel_y + 16 > framebuffer->height) {
        return;
    }

    const uint8_t* glyph = get_glyph_pointer(c);

    for (uint32_t g_row = 0; g_row < 16; g_row++) {
        uint8_t bits = glyph[g_row];
        size_t row_offset = (pixel_y + g_row) * u64_pitch + pixel_x;
        
        for (int bit = 0; bit < 8; bit++) {
            framebuffer_ptr[row_offset + (size_t)bit] = ((bits >> (7 - bit)) & 1) ? fg_color : bg_color;
        }
    }
}

void dogeio_clear_cells(void) {
    for (uint32_t i = 0; i < (TERMINAL_ROWS * TERMINAL_COLS); i++) {
        text_grid[i] = ' ';
        text_color_grid[i] = dogeio_color;
        bg_color_grid[i] = dogeio_background_color;
    }

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        log("[clear] framebuffer unavailable");
        return;
    }

    for (uint32_t y = 0; y < TERMINAL_ROWS; y++) {
        for (uint32_t x = 0; x < TERMINAL_COLS; x++) {
            dogeio_putchar_raw_glyph(' ', x, y);
        }
    }
}

/* bullshit information stuff, also setting stuff. */
uint64_t dogeio_screen_width(void) {
    return framebuffer_request.response->framebuffers[0]->width;
}

uint64_t dogeio_screen_height(void) {
    return framebuffer_request.response->framebuffers[0]->height;
}