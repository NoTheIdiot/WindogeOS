/*  FIH  */

// get some basic functions and types
#include <stdint.h>
#include <string.h>
#include <dogeio.h>
#include <bool.h>
#include <boot/limine.h>
#include <basicutil.h>
#include <stddef.h>
#include <boot/kernel.h>

// get the framebuffer array in the kernel
extern volatile struct limine_framebuffer_request framebuffer_request;

// terminal array exactly 8K bytes
char text_grid[TERMINAL_ROWS * TERMINAL_COLS];
uint32_t text_color_grid[TERMINAL_ROWS * TERMINAL_COLS];
uint32_t bg_color_grid[TERMINAL_ROWS * TERMINAL_COLS];

// other stuff
uint32_t cursor_x = 0;
uint32_t cursor_y = 1;
uint32_t dogeio_background_color = 0x000000;
uint32_t dogeio_text_color       = 0xFFCCCCCC;

static const uint32_t ansi_colors[16] = {
    0x000000, 0xAA0000, 0x00AA00, 0xAA5500, 0x0000AA, 0xAA00AA, 0x00AAAA, 0xAAAAAA,
    0x555555, 0xFF5555, 0x55FF55, 0xFFFF55, 0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFFFF
};

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

    uint32_t idx = y * TERMINAL_COLS + x;
    text_grid[idx] = c;
    text_color_grid[idx] = dogeio_text_color;
    bg_color_grid[idx] = dogeio_background_color;

    // point
    const uint8_t* glyph = terminal_font[(uint8_t)c];

    for (uint32_t g_row = 0; g_row < 16; g_row++) {
        uint8_t bits = glyph[g_row];
        size_t row_offset = (pixel_y + g_row) * u64_pitch + pixel_x;
        
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

void dogeio_text_clear() {
    for (uint32_t i = 0; i < (TERMINAL_ROWS * TERMINAL_COLS); i++) {
        text_grid[i] = ' ';
        text_color_grid[i] = dogeio_text_color;
        bg_color_grid[i] = dogeio_background_color;
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
    cursor_y = 1;
    menubar_draw();
}

void dogeio_text_clear_raw() {
    for (uint32_t i = 0; i < (TERMINAL_ROWS * TERMINAL_COLS); i++) {
        text_grid[i] = ' ';
        text_color_grid[i] = dogeio_text_color;
        bg_color_grid[i] = dogeio_background_color;
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

static void dogeio_text_scroll() {
    for (uint32_t row = 2; row < TERMINAL_ROWS; row++) {
        for (uint32_t col = 0; col < TERMINAL_COLS; col++) {
            uint32_t dst = ((row - 1) * TERMINAL_COLS) + col;
            uint32_t src = (row * TERMINAL_COLS) + col;
            text_grid[dst] = text_grid[src];
            text_color_grid[dst] = text_color_grid[src];
            bg_color_grid[dst] = bg_color_grid[src];
        }
    }

    uint32_t blank_row = TERMINAL_ROWS - 1;
    for (uint32_t col = 0; col < TERMINAL_COLS; col++) {
        uint32_t idx = blank_row * TERMINAL_COLS + col;
        text_grid[idx] = ' ';
        text_color_grid[idx] = dogeio_text_color;
        bg_color_grid[idx] = dogeio_background_color;
    }

    if (framebuffer_request.response != NULL && framebuffer_request.response->framebuffer_count >= 1) {
        uint32_t saved_text_color = dogeio_text_color;
        uint32_t saved_bg_color = dogeio_background_color;

        for (uint32_t row = 1; row < TERMINAL_ROWS; row++) {
            for (uint32_t col = 0; col < TERMINAL_COLS; col++) {
                uint32_t idx = row * TERMINAL_COLS + col;
                dogeio_text_color = text_color_grid[idx];
                dogeio_background_color = bg_color_grid[idx];
                dogeio_text_putchar(text_grid[idx], col, row);
            }
        }

        dogeio_text_color = saved_text_color;
        dogeio_background_color = saved_bg_color;
        menubar_draw();
    }
}

void dogeio_text_printchar(char c) {
    // newlines
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;

        if (cursor_y >= TERMINAL_ROWS) {
            dogeio_text_scroll();
            cursor_y = TERMINAL_ROWS - 1;
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
    // tabs (Fixed: renders ' ' spaces instead of passing raw '\t' to putchar)
    else if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            if (cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
                text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ' ';
                dogeio_text_putchar(' ', cursor_x, cursor_y);
                cursor_x++;
            }
        }

        if (cursor_x >= TERMINAL_COLS) {
            cursor_x = 0;
            cursor_y++;
            
            if (cursor_y >= TERMINAL_ROWS) {
                dogeio_text_scroll();
                cursor_y = TERMINAL_ROWS - 1;
            }
        }
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
                dogeio_text_scroll();
                cursor_y = TERMINAL_ROWS - 1;
            }
        }
    }
}

static size_t parse_ansi_escape(const char *str, size_t index) {
    index += 2; // Skip past '\033['
    int param = 0;

    while (str[index] >= '0' && str[index] <= '9') {
        param = param * 10 + (str[index] - '0');
        index++;
    }

    if (str[index] == 'm') {
        if (param == 0) {
            dogeio_text_color = 0xFFCCCCCC;       // Reset FG
            dogeio_background_color = 0x000000;   // Reset BG
        } else if (param >= 30 && param <= 37) {
            dogeio_text_color = ansi_colors[param - 30];
        } else if (param >= 90 && param <= 97) {
            dogeio_text_color = ansi_colors[param - 90 + 8];
        } else if (param >= 40 && param <= 47) {
            dogeio_background_color = ansi_colors[param - 40];
        } else if (param >= 100 && param <= 107) {
            dogeio_background_color = ansi_colors[param - 100 + 8];
        }
    } else if (str[index] == 'J' && param == 2) {
        dogeio_text_clear();
    }

    return index;
}

void dogeio_text_print(const char *str) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\033' && str[i + 1] == '[') {
            i = parse_ansi_escape(str, i);
            continue;
        }

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
            current_y += 1;
            continue;
        }

        if (current_x >= TERMINAL_COLS || current_y >= TERMINAL_ROWS) {
            break; 
        }

        uint32_t idx = current_y * TERMINAL_COLS + current_x;
        text_grid[idx] = c;
        text_color_grid[idx] = dogeio_text_color;
        bg_color_grid[idx] = dogeio_background_color;
        dogeio_text_putchar(c, current_x, current_y);
        current_x += 1;
    }

    dogeio_text_color = original_color;
}

void dogeio_text_color_change(uint32_t color) {
    dogeio_text_color = color;
}

void dogeio_text_background_change(uint32_t color) {
    dogeio_background_color = color;
}