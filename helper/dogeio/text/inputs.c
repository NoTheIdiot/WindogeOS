#include <dogeio.h>
#include <lowlevel.h>

extern uint32_t cursor_x; 
extern uint32_t cursor_y;

static const char layout_normal[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char layout_shifted[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

static int is_shift_active = 0;

char dogeio_text_getchar(void) {
    while (1) {
        if ((lowlevel_ports_inb(0x64) & 1) == 0) {
            continue; 
        }

        uint8_t scancode = lowlevel_ports_inb(0x60);

        if (scancode == 0x2A || scancode == 0x36) {
            is_shift_active = 1;
            continue;
        }

        if (scancode == 0xAA || scancode == 0xB6) {
            is_shift_active = 0;
            continue;
        }

        if (scancode & 0x80) {
            continue;
        }

        if (scancode < sizeof(layout_normal)) {
            char target_ascii = is_shift_active ? layout_shifted[scancode] : layout_normal[scancode];
            if (target_ascii != 0) {
                return target_ascii;
            }
        }
    }
}

void dogeio_text_input(const char *prompt, char *buffer, size_t max_len) {
    dogeio_text_print(prompt);

    size_t length = 0;

    while (1) {
        char key = dogeio_text_getchar();

        if (key == '\n') {
            buffer[length] = '\0';
            dogeio_text_putchar('\n', cursor_x, cursor_y); 
            return;
        }

        if (key == '\b') {
            if (length > 0) {
                length--;
                if (cursor_x >= 8) {
                    cursor_x -= 8;
                }
                dogeio_text_putchar(' ', cursor_x, cursor_y);
            }
            continue;
        }

        if (length < (max_len - 1)) {
            buffer[length] = key;
            length++;
            dogeio_text_putchar(key, cursor_x, cursor_y);
            cursor_x += 8;
        }
    }
}
