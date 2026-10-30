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

static const uint32_t brightness_factors[11] = {
    0, 26, 51, 77, 102, 128, 154, 179, 205, 230, 256
};

static int current_brightness_level = 10;

const uint8_t* current_font = vga_text;

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

static inline uint32_t apply_brightness(uint32_t color) {
    uint32_t factor = brightness_factors[current_brightness_level];
    if (factor == 256) return color;

    uint32_t rb = color & 0x00FF00FF;
    uint32_t ag = (color >> 8) & 0x00FF00FF;

    rb = (rb * factor) >> 8;
    ag = (ag * factor) >> 8;

    rb &= 0x00FF00FF;
    ag &= 0x00FF00FF;

    return rb | (ag << 8);
}

void dogeio_render_glyph(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    fg_color = apply_brightness(fg_color);
    bg_color = apply_brightness(bg_color);

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

void dogeio_set_brightness(int level) {
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    current_brightness_level = level;
}

void dogeio_brightness_increase(void) {
    if (current_brightness_level < 10) {
        dogeio_change_brightness_switch(current_brightness_level + 1);
    }
}

void dogeio_refresh_screen(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }
    for (uint32_t y = 0; y < TERMINAL_ROWS; y++) {
        for (uint32_t x = 0; x < TERMINAL_COLS; x++) {
            uint32_t idx = y * TERMINAL_COLS + x;
            dogeio_render_glyph(text_grid[idx], x, y, text_color_grid[idx], bg_color_grid[idx]);
        }
    }
}

void dogeio_change_brightness_switch(int new_level) {
    dogeio_set_brightness(new_level);
    dogeio_refresh_screen();
}

void dogeio_brightness_decrease(void) {
    if (current_brightness_level > 1) {
        dogeio_change_brightness_switch(current_brightness_level - 1);
    }
}

void dogeio_set_font(const uint8_t* font) {
    if (font != NULL) {
        current_font = font;
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

    fg_color = apply_brightness(fg_color);
    bg_color = apply_brightness(bg_color);

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

int dogeio_screen_width(void) {
    return (int)framebuffer_request.response->framebuffers[0]->width;
}

int dogeio_screen_height(void) {
    return (int)framebuffer_request.response->framebuffers[0]->height;
}
