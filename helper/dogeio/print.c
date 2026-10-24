#include <stdint.h>
#include <dogeio.h>
#include <bool.h>
#include <boot/kernel.h>
#include <boot/limine.h>
#include <stddef.h>
#include <basicutil.h>

extern volatile struct limine_framebuffer_request framebuffer_request;
extern const uint8_t* get_glyph_pointer(char c);
extern void dogeio_render_glyph(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
extern void dogeio_render_pixel_glyph(char c, uint32_t pixel_x, uint32_t pixel_y, uint32_t fg_color, uint32_t bg_color);
extern void dogeio_clear_cells(void);
extern void dogeio_putchar_raw_glyph(char c, uint32_t x, uint32_t y);
extern void dogeio_putchar_reverted_glyph(char c, uint32_t x, uint32_t y);
extern void dogeio_set_font(const uint8_t* font);
extern const uint32_t ansi_colors[16];
extern const uint8_t* current_font;

char text_grid[TERMINAL_ROWS * TERMINAL_COLS];
uint32_t text_color_grid[TERMINAL_ROWS * TERMINAL_COLS];
uint32_t bg_color_grid[TERMINAL_ROWS * TERMINAL_COLS];

uint32_t cursor_x = 0;
uint32_t cursor_y = 1;
bool     dogeio_cursor_visible = false;
uint32_t dogeio_background_color = COLOR_BLACK;
uint32_t dogeio_color = COLOR_GRAY;
bool     dogeio_video_mode = false;

uint32_t video_cursor_x = 0;
uint32_t video_cursor_y = 0;

void dogeio_set_video_mode(bool enabled) {
    dogeio_video_mode = enabled;
}

void dogeio_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    if (x >= framebuffer->width || y >= framebuffer->height) {
        return;
    }

    uint32_t* framebuffer_ptr = (uint32_t*)framebuffer->address;
    uint64_t u64_pitch = framebuffer->pitch / 4;
    framebuffer_ptr[y * u64_pitch + x] = color;
}

void dogeio_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    uint32_t max_x = x + width;
    uint32_t max_y = y + height;

    if (max_x > framebuffer->width) max_x = (uint32_t)framebuffer->width;
    if (max_y > framebuffer->height) max_y = (uint32_t)framebuffer->height;

    uint32_t* framebuffer_ptr = (uint32_t*)framebuffer->address;
    uint64_t u64_pitch = framebuffer->pitch / 4;

    for (uint32_t py = y; py < max_y; py++) {
        uint64_t row_offset = py * u64_pitch;
        for (uint32_t px = x; px < max_x; px++) {
            framebuffer_ptr[row_offset + px] = color;
        }
    }
}

void dogeio_putchar(char c, uint32_t x, uint32_t y) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    if (dogeio_video_mode) {
        dogeio_render_pixel_glyph(c, x, y, dogeio_color, dogeio_background_color);
        return;
    }

    if ((uint8_t)c >= 128) {
        c = ' '; 
    }

    if (x >= TERMINAL_COLS || y >= TERMINAL_ROWS) {
        return;
    }

    uint32_t idx = y * TERMINAL_COLS + x;
    text_grid[idx] = c;
    text_color_grid[idx] = dogeio_color;
    bg_color_grid[idx] = dogeio_background_color;

    dogeio_putchar_raw_glyph(c, x, y);

}

void dogeio_clear() {
    if (dogeio_video_mode) {
        if (framebuffer_request.response != NULL && framebuffer_request.response->framebuffer_count >= 1) {
            struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
            dogeio_draw_rect(0, 0, (uint32_t)framebuffer->width, (uint32_t)framebuffer->height, dogeio_background_color);
        }
        video_cursor_x = 0;
        video_cursor_y = 0;
        return;
    }

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

    for (uint32_t y = 1; y < TERMINAL_ROWS; y++) {
        for (uint32_t x = 0; x < TERMINAL_COLS; x++) {
            dogeio_putchar_raw_glyph(' ', x, y);
        }
    }
    cursor_y = 1;
    cursor_x = 0;
}

void dogeio_clear_raw() {
    if (dogeio_video_mode) {
        dogeio_clear();
        return;
    }
    dogeio_clear_cells();
    cursor_x = 0;
    cursor_y = 0;
}

static void dogeio_scroll() {
    if (dogeio_video_mode) {
        video_cursor_x = 0;
        video_cursor_y += 16;
        return;
    }

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
        text_color_grid[idx] = dogeio_color;
        bg_color_grid[idx] = dogeio_background_color;
    }

    if (framebuffer_request.response != NULL && framebuffer_request.response->framebuffer_count >= 1) {
        uint32_t saved_text_color = dogeio_color;
        uint32_t saved_bg_color = dogeio_background_color;

        for (uint32_t row = 1; row < TERMINAL_ROWS; row++) {
            for (uint32_t col = 0; col < TERMINAL_COLS; col++) {
                uint32_t idx = row * TERMINAL_COLS + col;
                dogeio_color = text_color_grid[idx];
                dogeio_background_color = bg_color_grid[idx];
                dogeio_putchar(text_grid[idx], col, row);
            }
        }

        dogeio_color = saved_text_color;
        dogeio_background_color = saved_bg_color;
        menubar_draw();
        if (dogeio_cursor_visible && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
            uint32_t idx = cursor_y * TERMINAL_COLS + cursor_x;
            dogeio_putchar_reverted_glyph(text_grid[idx], cursor_x, cursor_y);
        }
    }
}

void dogeio_printchar(char c) {
    if (dogeio_video_mode) {
        if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
            return;
        }
        struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

        if (c == '\n') {
            video_cursor_x = 0;
            video_cursor_y += 16;
        } else if (c == '\r') {
            video_cursor_x = 0;
        } else if (c == '\t') {
            video_cursor_x += 32; 
        } else {
            if (video_cursor_x + 8 > framebuffer->width) {
                video_cursor_x = 0;
                video_cursor_y += 16;
            }
            if (video_cursor_y + 16 > framebuffer->height) {
                video_cursor_y = 0; 
            }
            dogeio_render_pixel_glyph(c, video_cursor_x, video_cursor_y, dogeio_color, dogeio_background_color);
            video_cursor_x += 8;
        }
        return;
    }

    if (dogeio_cursor_visible && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
        uint32_t current_idx = cursor_y * TERMINAL_COLS + cursor_x;
        dogeio_putchar_raw_glyph(text_grid[current_idx], cursor_x, cursor_y);
    }

    if (c == '\n') {
        text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ' ';
        dogeio_putchar(' ', cursor_x, cursor_y);

        cursor_x = 0;
        cursor_y++;

        if (cursor_y >= TERMINAL_ROWS) {
            dogeio_scroll();
            cursor_y = TERMINAL_ROWS - 1;
        }
    }
    else if (c == '\b') {
        if (cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
            uint32_t old_idx = cursor_y * TERMINAL_COLS + cursor_x;
            dogeio_putchar(text_grid[old_idx], cursor_x, cursor_y);
        }

        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 1) { 
            cursor_y--;
            cursor_x = TERMINAL_COLS - 1;
        }
        
        text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ' ';
        dogeio_putchar(' ', cursor_x, cursor_y);
    } 
    else if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            if (cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
                text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ' ';
                dogeio_putchar(' ', cursor_x, cursor_y);
                cursor_x++;
            }
        }

        if (cursor_x >= TERMINAL_COLS) {
            cursor_x = 0;
            cursor_y++;
            
            if (cursor_y >= TERMINAL_ROWS) {
                dogeio_scroll();
                cursor_y = TERMINAL_ROWS - 1;
            }
        }
    }
    else {
        if (cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
            text_grid[cursor_y * TERMINAL_COLS + cursor_x] = c;
            dogeio_putchar(c, cursor_x, cursor_y);
            cursor_x++;
        }

        if (cursor_x >= TERMINAL_COLS) {
            cursor_x = 0;
            cursor_y++;
            
            if (cursor_y >= TERMINAL_ROWS) {
                dogeio_scroll();
                cursor_y = TERMINAL_ROWS - 1;
            }
        }
    }

    if (dogeio_cursor_visible && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
        uint32_t idx = cursor_y * TERMINAL_COLS + cursor_x;
        dogeio_putchar_reverted_glyph(text_grid[idx], cursor_x, cursor_y);
    }
}

void dogeio_cursor_show(void) {
    dogeio_cursor_visible = true;
    if (!dogeio_video_mode && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
        uint32_t idx = cursor_y * TERMINAL_COLS + cursor_x;
        dogeio_putchar_reverted_glyph(text_grid[idx], cursor_x, cursor_y);
    }
}

void dogeio_cursor_hide(void) {
    if (dogeio_cursor_visible && !dogeio_video_mode && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
        uint32_t idx = cursor_y * TERMINAL_COLS + cursor_x;
        dogeio_putchar_raw_glyph(text_grid[idx], cursor_x, cursor_y);
    }
    dogeio_cursor_visible = false;
}

void dogeio_video(void) {
    dogeio_video_mode = true;
}

void dogeio_text(void) {
    dogeio_video_mode = false;
}

static size_t parse_ansi_escape(const char *str, size_t index) {
    index += 2; 

    int params[4] = {0, 0, 0, 0};
    int param_count = 0;
    int current_param = 0;
    bool has_param = false;

    while (str[index] != '\0') {
        char ch = str[index];
        if (ch >= '0' && ch <= '9') {
            current_param = current_param * 10 + (ch - '0');
            has_param = true;
            index++;
        } else if (ch == ';') {
            if (param_count < 4) {
                params[param_count++] = has_param ? current_param : 0;
            }
            current_param = 0;
            has_param = false;
            index++;
        } else {
            if (param_count < 4) {
                params[param_count++] = has_param ? current_param : 0;
            }
            break;
        }
    }

    char cmd = str[index];

    if (dogeio_video_mode) {
        if (cmd == 'm') {
            int param = params[0];
            if (param == 0) {
                dogeio_color = 0xFFCCCCCC;       
                dogeio_background_color = 0x000000;   
            } else if (param >= 30 && param <= 37) {
                dogeio_color = ansi_colors[param - 30];
            } else if (param >= 90 && param <= 97) {
                dogeio_color = ansi_colors[param - 90 + 8];
            } else if (param >= 40 && param <= 47) {
                dogeio_background_color = ansi_colors[param - 40];
            } else if (param >= 100 && param <= 107) {
                dogeio_background_color = ansi_colors[param - 100 + 8];
            }
        } else if (cmd == 'H' || cmd == 'f') {
            int target_row = (params[0] == 0) ? 1 : params[0];
            int target_col = (params[1] == 0) ? 1 : params[1];
            video_cursor_x = (uint32_t)(target_col - 1) * 8;
            video_cursor_y = (uint32_t)(target_row - 1) * 16;
        }
        return str[index] == '\0' ? index - 1 : index;
    }

    uint32_t old_cursor_x = cursor_x;
    uint32_t old_cursor_y = cursor_y;
    bool was_visible = dogeio_cursor_visible;

    if (was_visible && old_cursor_x < TERMINAL_COLS && old_cursor_y < TERMINAL_ROWS) {
        uint32_t idx = old_cursor_y * TERMINAL_COLS + old_cursor_x;
        dogeio_putchar_raw_glyph(text_grid[idx], old_cursor_x, old_cursor_y);
    }

    if (cmd == 'm') {
        int param = params[0];
        if (param == 0) {
            dogeio_color = 0xFFCCCCCC;       
            dogeio_background_color = 0x000000;   
        } else if (param >= 30 && param <= 37) {
            dogeio_color = ansi_colors[param - 30];
        } else if (param >= 90 && param <= 97) {
            dogeio_color = ansi_colors[param - 90 + 8];
        } else if (param >= 40 && param <= 47) {
            dogeio_background_color = ansi_colors[param - 40];
        } else if (param >= 100 && param <= 107) {
            dogeio_background_color = ansi_colors[param - 100 + 8];
        }
    } else if (cmd == 'J') {
        int param = params[0];
        if (param == 2) {
            dogeio_clear();
        }
    } else if (cmd == 'A') { 
        int n = (params[0] == 0) ? 1 : params[0];
        if (cursor_y >= (uint32_t)n) {
            cursor_y -= (uint32_t)n;
        } else {
            cursor_y = 0;
        }
    } else if (cmd == 'B') { 
        int n = (params[0] == 0) ? 1 : params[0];
        cursor_y += (uint32_t)n;
        if (cursor_y >= TERMINAL_ROWS) {
            cursor_y = TERMINAL_ROWS - 1;
        }
    } else if (cmd == 'C') { 
        int n = (params[0] == 0) ? 1 : params[0];
        cursor_x += (uint32_t)n;
        if (cursor_x >= TERMINAL_COLS) {
            cursor_x = TERMINAL_COLS - 1;
        }
    } else if (cmd == 'D') { 
        int n = (params[0] == 0) ? 1 : params[0];
        if (cursor_x >= (uint32_t)n) {
            cursor_x -= (uint32_t)n;
        } else {
            cursor_x = 0;
        }
    } else if (cmd == 'H' || cmd == 'f') { 
        int target_row = (params[0] == 0) ? 1 : params[0];
        int target_col = (params[1] == 0) ? 1 : params[1];
        
        uint32_t new_y = (uint32_t)target_row - (uint32_t)1;
        uint32_t new_x = (uint32_t)target_col - (uint32_t)1;

        if (new_y < TERMINAL_ROWS) cursor_y = new_y;
        else cursor_y = TERMINAL_ROWS - 1;

        if (new_x < TERMINAL_COLS) cursor_x = new_x;
        else cursor_x = TERMINAL_COLS - 1;
    }

    if (was_visible && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
        uint32_t idx = cursor_y * TERMINAL_COLS + cursor_x;
        dogeio_putchar_reverted_glyph(text_grid[idx], cursor_x, cursor_y);
    }

    return str[index] == '\0' ? index - 1 : index;
}

void dogeio_print(const char *str) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }
    if (str == NULL) {
        return;
    }

    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\033' && str[i + 1] == '[') {
            i = parse_ansi_escape(str, i);
            continue;
        }

        dogeio_printchar(str[i]);
    }
}

void dogeio_println(const char *str) {
    dogeio_print(str);
    dogeio_print("\n");
}

void dogeio_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    if (dogeio_video_mode) {
        uint32_t original_color = dogeio_color;
        dogeio_color = text_color;
        
        uint32_t saved_x = video_cursor_x;
        uint32_t saved_y = video_cursor_y;
        
        video_cursor_x = x_pos; 
        video_cursor_y = y_pos;
        
        dogeio_print(str);
        
        video_cursor_x = saved_x;
        video_cursor_y = saved_y;
        dogeio_color = original_color;
        return;
    }

    uint32_t current_x = x_pos;
    uint32_t current_y = y_pos;

    uint32_t original_color = dogeio_color;
    dogeio_color = text_color;

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
        text_color_grid[idx] = dogeio_color;
        bg_color_grid[idx] = dogeio_background_color;
        dogeio_putchar(c, current_x, current_y);
        current_x += 1;
    }

    dogeio_color = original_color;
}

void dogeio_color_change(uint32_t color) {
    dogeio_color = color;
}

void dogeio_background_change(uint32_t color) {
    dogeio_background_color = color;
}

void dogeio_print_hex8(uint8_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[3] = { hex_chars[(val >> 4) & 0x0F], hex_chars[val & 0x0F], '\0' };
    dogeio_print(buf);
}

void dogeio_print_hex16(uint16_t val) {
    dogeio_print_hex8((uint8_t)(val >> 8));
    dogeio_print_hex8((uint8_t)(val & 0xFF));
}