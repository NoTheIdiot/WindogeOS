#include <dogeio.h>
#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <fonts.h>
#include <string.h>
#include <bool.h>
#include <limine.h>

extern volatile struct limine_framebuffer_request framebuffer_request;

char text_grid[TERMINAL_ROWS * TERMINAL_COLS];
uint32_t text_color_grid[TERMINAL_ROWS * TERMINAL_COLS];
uint32_t bg_color_grid[TERMINAL_ROWS * TERMINAL_COLS];

uint32_t cursor_x = 0;
uint32_t cursor_y = 0;
bool     cursor_on = false;
uint32_t background_color = COLOR_BLACK;
uint32_t text_color       = COLOR_GRAY;

uint32_t video_cursor_x = 0;
uint32_t video_cursor_y = 0;
const uint8_t* current_font   = vga_text;

const uint32_t ansi_colors[16] = {
    0x000000, 0xAA0000, 0x00AA00, 0xAA5500, 0x0000AA, 0xAA00AA, 0x00AAAA, 0xAAAAAA,
    0x555555, 0xFF5555, 0x55FF55, 0xFFFF55, 0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFFFF
};

// functions
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

void render_glyph(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) {
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
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

void putchar_raw_glyph(char c, uint32_t x, uint32_t y) {
    render_glyph(c, x, y, text_color, background_color);
}

void putchar_reverted_glyph(char c, uint32_t x, uint32_t y) {
    render_glyph(c, x, y, background_color, text_color);
}

void render_pixel_glyph(char c, uint32_t pixel_x, uint32_t pixel_y, uint32_t fg_color, uint32_t bg_color) {
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
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

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
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

void clear() {
    for (uint32_t i = 0; i < (TERMINAL_ROWS * TERMINAL_COLS); i++) {
        text_grid[i] = ' ';
        text_color_grid[i] = text_color;
        bg_color_grid[i] = background_color;
    }

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    for (uint32_t y = 0; y < TERMINAL_ROWS; y++) {
        for (uint32_t x = 0; x < TERMINAL_COLS; x++) {
            putchar_raw_glyph(' ', x, y);
        }
    }
    cursor_y = 0;
    cursor_x = 0;
}

void set_font(const uint8_t* font) {
    if (font != NULL) {
        current_font = font;
    }
}

void putchar(char c, uint32_t x, uint32_t y) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
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
    text_color_grid[idx] = text_color;
    bg_color_grid[idx] = background_color;

    putchar_raw_glyph(c, x, y);
}

static void scroll() {
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
        text_color_grid[idx] = text_color;
        bg_color_grid[idx] = background_color;
    }

    if (framebuffer_request.response != NULL && framebuffer_request.response->framebuffer_count >= 1) {
        uint32_t saved_text_color = text_color;
        uint32_t saved_bg_color = background_color;

        for (uint32_t row = 1; row < TERMINAL_ROWS; row++) {
            for (uint32_t col = 0; col < TERMINAL_COLS; col++) {
                uint32_t idx = row * TERMINAL_COLS + col;
                text_color = text_color_grid[idx];
                background_color = bg_color_grid[idx];
                putchar(text_grid[idx], col, row);
            }
        }

        text_color = saved_text_color;
        background_color = saved_bg_color;
        if (cursor_on && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
            uint32_t idx = cursor_y * TERMINAL_COLS + cursor_x;
            putchar_reverted_glyph(text_grid[idx], cursor_x, cursor_y);
        }
    }
}

void printchar(char c) {
    if (cursor_on && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
        uint32_t current_idx = cursor_y * TERMINAL_COLS + cursor_x;
        putchar_raw_glyph(text_grid[current_idx], cursor_x, cursor_y);
    }

    if (c == '\n') {
        text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ' ';
        putchar(' ', cursor_x, cursor_y);

        cursor_x = 0;
        cursor_y++;

        if (cursor_y >= TERMINAL_ROWS) {
            scroll();
            cursor_y = TERMINAL_ROWS - 1;
        }
    }
    else if (c == '\b') {
        if (cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
            uint32_t old_idx = cursor_y * TERMINAL_COLS + cursor_x;
            putchar(text_grid[old_idx], cursor_x, cursor_y);
        }

        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 1) { 
            cursor_y--;
            cursor_x = TERMINAL_COLS - 1;
        }
        
        text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ' ';
        putchar(' ', cursor_x, cursor_y);
    } 
    else if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            if (cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
                text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ' ';
                putchar(' ', cursor_x, cursor_y);
                cursor_x++;
            }
        }

        if (cursor_x >= TERMINAL_COLS) {
            cursor_x = 0;
            cursor_y++;
            
            if (cursor_y >= TERMINAL_ROWS) {
                scroll();
                cursor_y = TERMINAL_ROWS - 1;
            }
        }
    }
    else {
        if (cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
            text_grid[cursor_y * TERMINAL_COLS + cursor_x] = c;
            putchar(c, cursor_x, cursor_y);
            cursor_x++;
        }

        if (cursor_x >= TERMINAL_COLS) {
            cursor_x = 0;
            cursor_y++;
            
            if (cursor_y >= TERMINAL_ROWS) {
                scroll();
                cursor_y = TERMINAL_ROWS - 1;
            }
        }
    }

    if (cursor_on && cursor_x < TERMINAL_COLS && cursor_y < TERMINAL_ROWS) {
        uint32_t idx = cursor_y * TERMINAL_COLS + cursor_x;
        putchar_reverted_glyph(text_grid[idx], cursor_x, cursor_y);
    }
}

static size_t parse_ansi_escape(const char *str, size_t index) {
    index += 2; 

    int params[4];
    memset(params, 0, sizeof(params));
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

    uint32_t old_cursor_x = cursor_x;
    uint32_t old_cursor_y = cursor_y;
    bool was_visible = cursor_on;

    if (was_visible && old_cursor_x < TERMINAL_COLS && old_cursor_y < TERMINAL_ROWS) {
        uint32_t idx = old_cursor_y * TERMINAL_COLS + old_cursor_x;
        putchar_raw_glyph(text_grid[idx], old_cursor_x, old_cursor_y);
    }

    if (cmd == 'm') {
        int param = params[0];
        if (param == 0) {
            text_color = 0xFFCCCCCC;       
            background_color = 0x000000;   
        } else if (param >= 30 && param <= 37) {
            text_color = ansi_colors[param - 30];
        } else if (param >= 90 && param <= 97) {
            text_color = ansi_colors[param - 90 + 8];
        } else if (param >= 40 && param <= 47) {
            background_color = ansi_colors[param - 40];
        } else if (param >= 100 && param <= 107) {
            background_color = ansi_colors[param - 100 + 8];
        }
    } else if (cmd == 'J') {
        int param = params[0];
        if (param == 2) {
            clear();
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
        putchar_reverted_glyph(text_grid[idx], cursor_x, cursor_y);
    }

    return str[index] == '\0' ? index - 1 : index;
}

void print(char *str) {
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

        printchar(str[i]);
    }
}

void println(char *str) {
    print(str);
    print("\n");
}

void cursor_show(void) {
    cursor_on = true;
}

void cursor_hide(void) {
    cursor_on = false;
}