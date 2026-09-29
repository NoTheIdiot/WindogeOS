#include <stdint.h>
#include <bool.h>
#include <stddef.h>
#include <basicutil.h>
#include <dogeio.h>
#include <string.h>
#include <time.h>
#include <boot/kernel.h>

const char map_lower[] = {
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,   0,
   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
   'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

const char map_upper[] = {
    0,  0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
    0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,
   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
   'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

extern char text_grid[TERMINAL_ROWS * TERMINAL_COLS];

static bool global_shift_pressed = false;
static bool global_ctrl_pressed  = false;

uint16_t dogeio_get_key(void) {
    char last_seen_time[16] = {0};
    bool escaped = false;

    while (1) {
        while ((ports_inb(0x64) & 1) == 0) {
            char* current_time = time_get();
            if (str_strcmp(current_time, last_seen_time) != 0) {
                menubar_draw();
                for (int i = 0; i < 15; i++) {
                    last_seen_time[i] = current_time[i];
                    if (current_time[i] == '\0') {
                        break;
                    }
                }
                last_seen_time[15] = '\0';
            }
        }

        uint8_t code = ports_inb(0x60);

        if (code == 0xE0) {
            escaped = true;
            continue;
        }

        if (code == 0x2A || code == 0x36) {
            global_shift_pressed = true;
            escaped = false;
            continue;
        }
        
        if (code == 0xAA || code == 0xB6) {
            global_shift_pressed = false;
            escaped = false;
            continue;
        }
        
        if (code == 0x1D) {
            global_ctrl_pressed = true;
            escaped = false;
            continue;
        }

       
        if (code == 0x9D) {
            global_ctrl_pressed = false;
            escaped = false;
            continue;
        }
        
        if (code & 0x80) {
            escaped = false;
            continue;
        }

        if (escaped) {
            escaped = false;
            if (code == 0x48) return KEY_UP;
            if (code == 0x50) return KEY_DOWN;
            if (code == 0x4B) return KEY_LEFT;
            if (code == 0x4D) return KEY_RIGHT;
            continue;
        }

        if (code == 0x1C) return KEY_ENTER;
        if (code == 0x0E) return KEY_BACKSPACE;

        if (code == 0x48) return KEY_UP;
        if (code == 0x50) return KEY_DOWN;
        if (code == 0x4B) return KEY_LEFT;
        if (code == 0x4D) return KEY_RIGHT;

        if (code < 58) {
            char c = global_shift_pressed ? map_upper[code] : map_lower[code];
            if (c != 0) {
                if (global_ctrl_pressed) {
                    if (c >= 'a' && c <= 'z') {
                        return (uint16_t)(c - 'a' + 1);
                    } else if (c >= 'A' && c <= 'Z') {
                        return (uint16_t)(c - 'A' + 1);
                    }
                }
                return (uint16_t)c;
            }
        }
    }
    return KEY_UNKNOWN;
}

void dogeio_text_input(const char *prompt, char *buffer, size_t max_size) {
    if (prompt != NULL) {
        dogeio_text_print(prompt);
    }

    size_t len = 0;
    size_t pos = 0;
    buffer[0] = '\0';

    dogeio_text_cursor_show();

    while (1) {
        uint16_t key = dogeio_get_key();

        if (key == KEY_ENTER) {
            dogeio_text_print("\n");
            break;
        }

        if (key == KEY_LEFT) {
            if (pos > 0) {
                dogeio_text_cursor_hide();
                pos--;
                if (cursor_x > 0) {
                    cursor_x--;
                } else if (cursor_y > 1) {
                    cursor_y--;
                    cursor_x = TERMINAL_COLS - 1;
                }
                dogeio_text_cursor_show();
            }
            continue;
        }

        if (key == KEY_TAB) {
            if (len + 8 < max_size) {
                dogeio_text_cursor_hide();
                
                for (size_t i = len + 8; i > pos + 7; i--) {
                    buffer[i] = buffer[i - 8];
                }
                
                for (size_t i = 0; i < 8; i++) {
                    buffer[pos + i] = ' ';
                }
                
                len += 8;
                buffer[len] = '\0';

                
                uint32_t saved_x = cursor_x;
                uint32_t saved_y = cursor_y;

                for (size_t i = pos; i < len; i++) {
                    char ch = buffer[i];
                    text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ch;
                    dogeio_text_putchar(ch, cursor_x, cursor_y);
                    cursor_x++;
                    if (cursor_x >= TERMINAL_COLS) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }
                
                cursor_x = saved_x;
                cursor_y = saved_y;
                for (size_t i = 0; i < 8; i++) {
                    cursor_x++;
                    if (cursor_x >= TERMINAL_COLS) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }
                pos += 8;

                dogeio_text_cursor_show();
            }
            continue;
        }

        if (key == KEY_RIGHT) {
            if (pos < len) {
                dogeio_text_cursor_hide();
                pos++;
                cursor_x++;
                if (cursor_x >= TERMINAL_COLS) {
                    cursor_x = 0;
                    cursor_y++;
                }
                dogeio_text_cursor_show();
            }
            continue;
        }

        if (key == KEY_BACKSPACE) {
            if (pos > 0) {
                dogeio_text_cursor_hide();
                
                for (size_t i = pos - 1; i < len; i++) {
                    buffer[i] = buffer[i + 1];
                }
                len--;
                pos--;
                buffer[len] = '\0';

                if (cursor_x > 0) {
                    cursor_x--;
                } else if (cursor_y > 1) {
                    cursor_y--;
                    cursor_x = TERMINAL_COLS - 1;
                }

                uint32_t saved_x = cursor_x;
                uint32_t saved_y = cursor_y;

                for (size_t i = pos; i <= len; i++) {
                    char c = (i < len) ? buffer[i] : ' ';
                    text_grid[cursor_y * TERMINAL_COLS + cursor_x] = c;
                    dogeio_text_putchar(c, cursor_x, cursor_y);
                    cursor_x++;
                    if (cursor_x >= TERMINAL_COLS) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }

                cursor_x = saved_x;
                cursor_y = saved_y;
                dogeio_text_cursor_show();
            }
            continue;
        }

        if (key >= 32 && key < 127) {
            char c = (char)key;
            if (len + 1 < max_size) {
                dogeio_text_cursor_hide();

                for (size_t i = len + 1; i > pos; i--) {
                    buffer[i] = buffer[i - 1];
                }
                buffer[pos] = c;
                len++;
                pos++;
                buffer[len] = '\0';

                uint32_t saved_x = cursor_x;
                uint32_t saved_y = cursor_y;

                for (size_t i = pos - 1; i < len; i++) {
                    char ch = buffer[i];
                    text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ch;
                    dogeio_text_putchar(ch, cursor_x, cursor_y);
                    cursor_x++;
                    if (cursor_x >= TERMINAL_COLS) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }

                cursor_x = saved_x;
                cursor_y = saved_y;
                
                cursor_x++;
                if (cursor_x >= TERMINAL_COLS) {
                    cursor_x = 0;
                    cursor_y++;
                }

                dogeio_text_cursor_show();
            }
            continue;
        }
    }

    dogeio_text_cursor_hide();
    buffer[len] = '\0';
}