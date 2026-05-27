/*
Make sure for a optional VBE and VGA text mode in the future
settings command soon
*/

// include files
#include <stdint.h>
#include <stddef.h>
#include "consts.h"
#include "ports.h"
#include "dogeio_vbe.h"
#include "vbe.h"

extern uint8_t vbe_initialized;

// cursor variables
int cursor_x = 0;
int cursor_y = 0;

// scancode
char scan_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

char scan_to_ascii_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

// you must put this FIRST
void dogeio_update_cursor(int x, int y) {
    uint16_t pos = y * 80 + x;
    ports_outb(0x3D4, 0x0F);
    ports_outb(0x3D5, (uint8_t)(pos & 0xFF));
    ports_outb(0x3D4, 0x0E);
    ports_outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}


// scroll
void dogeio_scroll() {
    uint16_t* vga_buffer = (uint16_t*)0xB8000;
    int last_row_start = (MAX_ROWS - 1) * MAX_COLS;

    for (int i = 0; i < last_row_start; i++) {
        vga_buffer[i] = vga_buffer[i + MAX_COLS];
    }

    // 2. Clear the last row (fill with spaces)
    uint16_t blank = (0x07 << 8) | ' ';
    for (int i = last_row_start; i < MAX_ROWS * MAX_COLS; i++) {
        vga_buffer[i] = blank;
    }

    cursor_y = MAX_ROWS - 1;
    cursor_x = 0;
}

// places a char in the screen
void dogeio_putchar(char input) {
    if (vbe_initialized) {
        dogeio_putchar_graphics(input);
        return;
    }

	volatile char* video_address = (volatile char*) VRAM_ADDRESS;
	int offset = (cursor_y * 80 + cursor_x) * 2;
	video_address[offset] = input;
	video_address[offset + 1] = DOGE_COLOR;
}

// prints a string by looping dogeio_putchar
void dogeio_print(char* string) {
    if (vbe_initialized) {
        dogeio_print_graphics(string);
        return;
    }

	for (int i = 0; string[i] != '\0'; i++) {
		if (string[i] == '\n') {
			cursor_x = 0;
			cursor_y++;
            dogeio_update_cursor(cursor_x, cursor_y);
		} else {
			dogeio_putchar(string[i]);
			cursor_x++;
            dogeio_update_cursor(cursor_x, cursor_y);
		}

		if (cursor_x >= MAX_COLS) {
			cursor_x = 0;
			cursor_y++;
            dogeio_update_cursor(cursor_x, cursor_y);
		}

		if (cursor_y >= MAX_ROWS) {
			dogeio_scroll();
		}
	}
}

// print a string with a newline
void dogeio_println(char* string) {
    if (vbe_initialized) {
        dogeio_println_graphics(string);
        return;
    }

    dogeio_print(string);
    dogeio_print("\n");
}

// clears the screen, without scrolling
void dogeio_clear_screen() {
    if (vbe_initialized) {
        dogeio_clear_screen_graphics();
        return;
    }

	for (int y = 0; y < MAX_ROWS; y++) {
		for (int x = 0; x < MAX_COLS; x++) {
			cursor_x = x;
			cursor_y = y;
			dogeio_putchar(' ');
		}
	}

	cursor_x = 0;
    cursor_y = 0;
    dogeio_update_cursor(0, 0);
}

// get inputs
void dogeio_input(char* buffer, int max_len, uint8_t color) {
    if (vbe_initialized) {
        dogeio_input_graphics(buffer, max_len, color);
        return;
    }

    int i = 0;
    int shift = 0;
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;

    while (i < max_len - 1) {
        while (!(ports_inb(0x64) & 0x01));
        uint8_t sc = ports_inb(0x60);

        if (sc == 0x2A || sc == 0x36) { shift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift = 0; continue; }
        if (sc & 0x80) continue;
        if (sc == 0x01) break;

        char c = (shift) ? scan_to_ascii_shift[sc] : scan_to_ascii[sc];
        if (!c) continue;

        if (c == '\n') {
            buffer[i] = '\0';
            cursor_x = 0;
            cursor_y++;
            return;
        }
        else if (c == '\b') {
            if (i > 0) {
                i--;
                buffer[i] = '\0';
                if (cursor_x > 0) {
                    cursor_x--;
                } else if (cursor_y > 0) {
                    cursor_y--;
                    cursor_x = 79;
                }
                vga[cursor_y * 80 + cursor_x] = (uint16_t)' ' | (uint16_t)color << 8;
            }
        }
        else {
            buffer[i++] = c;
            vga[cursor_y * 80 + cursor_x] = (uint16_t)c | (uint16_t)color << 8;
            cursor_x++;

            if (cursor_x >= 80) {
                cursor_x = 0;
                cursor_y++;
            }
        }

        if (cursor_y >= 25) cursor_y = 24;
        dogeio_update_cursor(cursor_x, cursor_y);
    }
    buffer[i] = '\0';
}
