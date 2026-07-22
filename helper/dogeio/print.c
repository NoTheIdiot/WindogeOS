// get some basic functions and types
#include <stdint.h>
#include <dogeio.h>
#include <bool.h>
#include <boot/limine.h>
#include <basicutil.h>
#include <stddef.h>

// get the framebuffer array in the kernel
extern volatile struct limine_framebuffer_request framebuffer_request;

// macros over consts >:)
// this is double of vga text
#define TERMINAL_COLS 160
#define TERMINAL_ROWS 50

// terminal array exactly 8K bytes
char text_grid[TERMINAL_ROWS * TERMINAL_COLS];

// other stuff
uint32_t cursor_x = 0;
uint32_t cursor_y = 0;
uint32_t dogeio_background_color = 0x000000;
uint32_t dogeio_text_color       = 0xffffff;

// place a char, obviously.
void dogeio_text_putchar(char c, uint32_t x, uint32_t y) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    if ((uint8_t)c >= 128) {
        c = ' '; 
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    uint32_t* framebuffer_ptr = (uint32_t*)framebuffer->address;

    // convert grid coordinates to raw pixels
    uint32_t pixel_x = x * 8;
    uint32_t pixel_y = y * 16;
    uint64_t u64_pitch = framebuffer->pitch / 4;

    // check if out of bounds and if is, throw it to the void
    if (pixel_x + 8 > framebuffer->width || pixel_y + 16 > framebuffer->height) {
        return;
    }

    // point
    const uint8_t* glyph = terminal_font[(uint8_t)c];

    // idk why this works but it works
    for (uint32_t g_row = 0; g_row < 16; g_row++) {
        uint8_t bits = glyph[g_row];
        size_t row_offset = (pixel_y + g_row) * u64_pitch + pixel_x;
        
        // loop.
        framebuffer_ptr[row_offset + 0] = ((bits >> 7) & 1) ? dogeio_text_color : dogeio_background_color;
        framebuffer_ptr[row_offset + 1] = ((bits >> 6) & 1) ? dogeio_text_color : dogeio_background_color;
        framebuffer_ptr[row_offset + 2] = ((bits >> 5) & 1) ? dogeio_text_color : dogeio_background_color;
        framebuffer_ptr[row_offset + 3] = ((bits >> 4) & 1) ? dogeio_text_color : dogeio_background_color;
        framebuffer_ptr[row_offset + 4] = ((bits >> 3) & 1) ? dogeio_text_color : dogeio_background_color;
        framebuffer_ptr[row_offset + 5] = ((bits >> 2) & 1) ? dogeio_text_color : dogeio_background_color;
        framebuffer_ptr[row_offset + 6] = ((bits >> 1) & 1) ? dogeio_text_color : dogeio_background_color;
        framebuffer_ptr[row_offset + 7] = (bits & 1)        ? dogeio_text_color : dogeio_background_color;
    }
}

// func() better than func(void)
void dogeio_text_clear() {
    for (uint32_t i = 0; i < (TERMINAL_ROWS * TERMINAL_COLS); i++) {
        text_grid[i] = ' ';
    }
    
    if (framebuffer_request.response != NULL && framebuffer_request.response->framebuffer_count >= 1) {
        struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
        uint32_t* fb_ptr = (uint32_t*)framebuffer->address;
        size_t total_pixels = (framebuffer->pitch / 4) * framebuffer->height;
        
        for (size_t i = 0; i < total_pixels; i++) {
            fb_ptr[i] = dogeio_background_color;
        }
    }

    cursor_x = 0;
    cursor_y = 0;
}

void dogeio_text_printchar(char c) {
    // newlines
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;

        if (cursor_y >= TERMINAL_ROWS) {
            dogeio_text_clear();
        }
    }
    // backspaces
    else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) { 
            cursor_y--;
            cursor_x = TERMINAL_COLS - 1;
        }
        
        text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ' ';
        dogeio_text_putchar(' ', cursor_x, cursor_y);
    }
    // standard printable characters path
    else {
        if (cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
            text_grid[cursor_y * TERMINAL_COLS + cursor_x] = c;
            dogeio_text_putchar(c, cursor_x, cursor_y);
            cursor_x++;
        }

        if (cursor_x >= TERMINAL_COLS) {
            cursor_x = 0;
            cursor_y++;
            
            if (cursor_y >= TERMINAL_ROWS) {
                dogeio_text_clear();
            }
        }
    }
}

void dogeio_text_print(const char *str) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    for (size_t i = 0; str[i] != '\0'; i++) {
        dogeio_text_printchar(str[i]);
    }
}

void dogeio_text_println(const char *str) {
    dogeio_text_print(str);
    dogeio_text_print("\n");
}

void dogeio_text_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    uint32_t current_x = x_pos;
    uint32_t current_y = y_pos;

    uint32_t original_color = dogeio_text_color;
    dogeio_text_color = text_color;

    for (size_t i = 0; str[i] != '\0'; i++) {
        char c = str[i];

        if (c == '\n') {
            current_x = x_pos;
            current_y += 1; // increment by 1 text matrix row
            continue;
        }

        if (current_x >= TERMINAL_COLS || current_y >= TERMINAL_ROWS) {
            break; 
        }

        dogeio_text_putchar(c, current_x, current_y);
        current_x += 1;
    }

    dogeio_text_color = original_color;
}

void dogeio_text_color_change(uint32_t color) {
	dogeio_text_color = color;
}