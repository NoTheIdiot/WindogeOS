#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <limine.h>

extern volatile struct limine_framebuffer_request framebuffer_request;

uint32_t cursor_x = 0;
uint32_t cursor_y = 0;
uint32_t dogeio_text_bcolor = 0x000000;
uint32_t dogeio_text_tcolor = 0xffffff;

void dogeio_text_putchar(char c, uint32_t x_pos, uint32_t y_pos) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = (uint32_t *)fb->address;
    
    if (x_pos >= fb->width || y_pos >= fb->height) {
        return;
    }

    const uint8_t *glyph = terminal_font[(uint8_t)c];

    for (int row = 0; row < 16; row++) {
        if (y_pos + row >= fb->height) {
            break;
        }

        uint8_t bits = glyph[row];
        
        for (int col = 0; col < 8; col++) {
            if (x_pos + col >= fb->width) {
                break;
            }

            if ((bits >> (7 - col)) & 1) {
                size_t pixel_index = (y_pos + row) * (fb->pitch / 4) + (x_pos + col);
                fb_ptr[pixel_index] = dogeio_text_tcolor;
            }
        }
    }
}

void dogeio_text_printchar(char c) {
    dogeio_text_putchar(c, cursor_x, cursor_y);
}

void dogeio_text_print(const char *str) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = (uint32_t *)fb->address;

    for (size_t i = 0; str[i] != '\0'; i++) {
        char c = str[i];

        if (c == '\n') {
            cursor_x = 0;
            cursor_y += 16;
            
            if (cursor_y + 16 >= fb->height) {
                dogeio_text_clear();
            }
            continue;
        }

        if (c == '\b') {
            if (cursor_x >= 8) {
                cursor_x -= 8;
            } else if (cursor_y >= 16) {
                cursor_y -= 16;
                cursor_x = ((fb->width / 8) * 8) - 8;
            } else {
                continue;
            }

            for (int row = 0; row < 16; row++) {
                if (cursor_y + row >= fb->height) break;
                for (int col = 0; col < 8; col++) {
                    if (cursor_x + col >= fb->width) break;
                    size_t pixel_index = (cursor_y + row) * (fb->pitch / 4) + (cursor_x + col);
                    fb_ptr[pixel_index] = dogeio_text_bcolor;
                }
            }
            continue;
        }

        if (cursor_y + 16 >= fb->height) {
            dogeio_text_clear();
        }

        dogeio_text_putchar(c, cursor_x, cursor_y);

        cursor_x += 8;

        if (cursor_x + 8 >= fb->width) {
            cursor_x = 0;
            cursor_y += 16;
        }

        if (cursor_y + 16 >= fb->height) {
            dogeio_text_clear();
        }
    }
}

void dogeio_text_println(const char* str) {
    dogeio_text_print(str);
    dogeio_text_print("\n");
}

void dogeio_text_clear() {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = (uint32_t *)fb->address;
    size_t total_pixels = (fb->pitch / 4) * fb->height;

    for (size_t i = 0; i < total_pixels; i++) {
        fb_ptr[i] = dogeio_text_bcolor;
    }

    cursor_x = 0;
    cursor_y = 0;
}

void dogeio_text_change_color(uint32_t color) {
    dogeio_text_tcolor = color;
    dogeio_text_clear();
}