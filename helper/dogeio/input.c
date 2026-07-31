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

void dogeio_text_input(const char *prompt, char *buffer, size_t max_size) {
    if (prompt != NULL) {
        dogeio_text_print(prompt);
    }

    size_t index = 0;
    bool shift_pressed = false;
    char last_seen_time[16] = {0};

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

        if (code == 0x2A || code == 0x36) {
            shift_pressed = true;
            continue;
        }

        if (code == 0xAA || code == 0xB6) {
            shift_pressed = false;
            continue;
        }

        if (code & 0x80) {
            continue;
        }

        if (code == 0x1C) {
            dogeio_text_print("\n");
            break;
        }

        if (code == 0x0E) {
            if (index > 0) {
                index--;
                buffer[index] = '\0';
                dogeio_text_print("\b");
            }
            continue;
        }

        if (code < 58) {
            char c = shift_pressed ? map_upper[code] : map_lower[code];

            if (c != 0) {
                if (index < (max_size - 1)) {
                    buffer[index++] = c;
                    char str[] = {c, '\0'};
                    dogeio_text_print(str);
                }
            }
        }
    }

    buffer[index] = '\0';
}
