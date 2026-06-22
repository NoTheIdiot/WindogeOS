// include files
#include <stdint.h>
#include <stddef.h>
#include <consts.h>
#include <ports.h>
#include <dogeio_vbe.h>
#include <vbe.h>

// global variables
int cursor_x = 0;               // x position of the text cursor
int cursor_y = 0;               // y position of the text cursor
uint8_t terminal_color = DOGE_COLOR;

// the scancode list for inputs
const char scan_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

// the scancode list but for when shift is on
const char scan_to_ascii_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

/******************************************************
 * BASIC VGA TEXT OUTPUT OR VBE WRAPPER IS INITIALIZED
******************************************************/

// cursor tracking movement
void dogeio_update_cursor(int x, int y) {
    uint16_t pos = y * MAX_COLS + x;   
    ports_outb(0x3D4, 0x0F);
    ports_outb(0x3D5, (uint8_t)(pos & 0xFF));
    ports_outb(0x3D4, 0x0E);
    ports_outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// to scroll through the screen
void dogeio_scroll() {
    // run this if vbe is present
    if (vbe_initialized) {
        // Future home for VBE Blit/Memmove scrolling
        return;                   
    }

    // run this if vbe not available
    uint16_t* vga_buffer = (uint16_t*)0xB8000;
    int last_row_start = (MAX_ROWS - 1) * MAX_COLS;

    // Shift screen up
    for (int i = 0; i < last_row_start; i++) {
        vga_buffer[i] = vga_buffer[i + MAX_COLS];
    }

    // Clear bottom row with standard space cells
    uint16_t blank = (0x07 << 8) | ' ';
    for (int i = last_row_start; i < MAX_ROWS * MAX_COLS; i++) {
        vga_buffer[i] = blank;
    }

    cursor_y = MAX_ROWS - 1;
    cursor_x = 0;
    dogeio_update_cursor(cursor_x, cursor_y);
}

// puts a char in the display
void dogeio_putchar(char input, int x_position, int y_position) {
    if (vbe_initialized) {
        dogeio_putchar_vbe(input);
        return;
    }

    // Explicit cast to prevent -Wextra pointer arithmetic optimization issues
    volatile uint8_t* video_address = (volatile uint8_t*)VRAM_ADDRESS;
    int vga_offset = (y_position * MAX_COLS + x_position) * 2;
    
    video_address[vga_offset] = (uint8_t)input;
    video_address[vga_offset + 1] = terminal_color;
}

// prints a string in the display
void dogeio_print(char* string) {
    if (vbe_initialized) {
        dogeio_print_vbe(string);
        return;
    }

    for (int i = 0; string[i] != '\0'; i++) {
        if (string[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            dogeio_putchar(string[i], cursor_x, cursor_y);
            cursor_x++;
        }

        // Handle line wrapping cleanly
        if (cursor_x >= MAX_COLS) {
            cursor_x = 0;
            cursor_y++;
        }

        // Trigger scroll if we hit boundary conditions
        if (cursor_y >= MAX_ROWS) {
            dogeio_scroll();
            // Scroll reset x to 0, so we don't need to overwrite it here
        }
    }
    // Perform a single unified hardware cursor update after printing string block
    dogeio_update_cursor(cursor_x, cursor_y);
}

// prints a string but with a newline
void dogeio_println(char* string) {
    if (vbe_initialized) {
        dogeio_println_vbe(string);
        return;
    }

    dogeio_print(string);
    dogeio_print("\n");
}

// clears the display
void dogeio_clear_screen() {
    if (vbe_initialized) {
        dogeio_clear_screen_vbe();
        return;
    }

    // High speed block loop wipe
    for (int y = 0; y < MAX_ROWS; y++) {
        for (int x = 0; x < MAX_COLS; x++) {
            dogeio_putchar(' ', x, y);
        }
    }

    cursor_x = 0;
    cursor_y = 0;
    dogeio_update_cursor(0, 0);
}

/*****************************************************
 * BASIC USER INPUT
 *****************************************************/

void dogeio_input(char* buffer, int max_len, uint8_t color) {
    if (vbe_initialized) {
        dogeio_input_vbe(buffer, max_len, color);
        return;
    }

    int i = 0;
    int shift = 0;
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;

    while (i < max_len - 1) {
        // Non-blocking status port evaluation loop
        while (!(ports_inb(0x64) & 0x01));
        uint8_t sc = ports_inb(0x60);

        // Track shift states safely
        if (sc == 0x2A || sc == 0x36) { shift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift = 0; continue; }
        if (sc & 0x80) continue; // Ignore standard release codes
        if (sc == 0x01) break;   // Escape code exit hook

        if (sc >= sizeof(scan_to_ascii)) continue;
        char c = (shift) ? scan_to_ascii_shift[sc] : scan_to_ascii[sc];
        if (!c) continue;

        if (c == '\n') {
            buffer[i] = '\0';
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= MAX_ROWS) {
                dogeio_scroll();
            } else {
                dogeio_update_cursor(cursor_x, cursor_y);
            }
            return;
        } 
        else if (c == '\b') {
            if (i > 0) {
                i--;
                buffer[i] = '\0';
                
                // Track visual boundary wrapping backward safely
                if (cursor_x > 0) {
                    cursor_x--;
                } else if (cursor_y > 0) {
                    cursor_y--;
                    cursor_x = MAX_COLS - 1;
                }
                // Clear out the cell using user configuration color parameters
                vga[cursor_y * MAX_COLS + cursor_x] = (uint16_t)' ' | ((uint16_t)color << 8);
            }
        } 
        else {
            buffer[i++] = c;
            vga[cursor_y * MAX_COLS + cursor_x] = (uint16_t)c | ((uint16_t)color << 8);
            cursor_x++;

            if (cursor_x >= MAX_COLS) {
                cursor_x = 0;
                cursor_y++;
            }
        }

        // Unified terminal overflow check
        if (cursor_y >= MAX_ROWS) {
            dogeio_scroll();
        } else {
            dogeio_update_cursor(cursor_x, cursor_y);
        }
    }
    buffer[i] = '\0';
}
