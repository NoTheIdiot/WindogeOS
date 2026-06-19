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
char scan_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

// the scancode list but for when shift is on
char scan_to_ascii_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

/******************************************************
 * BASIC VGA TEXT OUTPUT OR VBE WRAPPER IS INITIALIZED
******************************************************/


// cursor tracking movment
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
        return;                   // uhh i forgot to add a vbe scrolling function
    }

    // run this if vbe not avaliable
    uint16_t* vga_buffer = (uint16_t*)0xB8000;
    int last_row_start = (MAX_ROWS - 1) * MAX_COLS;

    for (int i = 0; i < last_row_start; i++) {
        vga_buffer[i] = vga_buffer[i + MAX_COLS];
    }

    uint16_t blank = (0x07 << 8) | ' ';
    for (int i = last_row_start; i < MAX_ROWS * MAX_COLS; i++) {
        vga_buffer[i] = blank;
    }

    cursor_y = MAX_ROWS - 1;
    cursor_x = 0;
}

// puts a char in the display
void dogeio_putchar(char input, int x_position, int y_position) {
    if (vbe_initialized) {
        // will ingnore the 2nd and 3rd arguments
        dogeio_putchar_vbe(input);
        return;
    }

    // do this so the compile doesn't try to optimize the code here,
    // which can break a ton of stuff
    volatile char* video_adress = (volatile char*) VRAM_ADDRESS;
    // calculate offset to get the position
    int vga_offset = (y_position * MAX_COLS + x_position) * 2;
    // set the calculated position to the inputed char
    video_adress[vga_offset] = input;
    // set the char's color 
    video_adress[vga_offset + 1] = terminal_color;
}

// prints a string in the display
void dogeio_print(char* string) {
    // run this is vbe exists
    if (vbe_initialized) {
        dogeio_print_vbe(string);
        // if i dont do this, it will continue to print the vga section anyway
        return;
    }

    for (int i = 0; string[i] != 0; i++) {
        // is the char in i in string, then push down the cursor
        if (string[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
            // change the cursor position to the virtual one
            dogeio_update_cursor(cursor_x, cursor_y);
        } else {
			dogeio_putchar(string[i], cursor_x, cursor_y);
			cursor_x++;
            dogeio_update_cursor(cursor_x, cursor_y);
		}

        // put text to the next line is it overflows
		if (cursor_x >= MAX_COLS) {
			cursor_x = 0;
			cursor_y++;
            dogeio_update_cursor(cursor_x, cursor_y);
		}

        // if y overflows, then scroll
        if (cursor_y >= MAX_ROWS) {
			dogeio_scroll();
		}
    }
}

// prints a string but with a newline
void dogeio_println(char* string) {
    // you all know the drill at this point
    if (vbe_initialized) {
        dogeio_println_vbe(string);
        // if there is no return then it will execute the vga 
        // part
        return;
    }

    // used "" instead of '' because i used -Werror and -Wextra to catch
    // potential bugs, one of them is partial type mismatch.
    dogeio_print(string);
    dogeio_print("\n");
}

// clears the display
void dogeio_clear_screen() {
    // if you don't know at this point, this runs if vbe is exisiting (unlike
    // my sense of will)
    if (vbe_initialized) {
        dogeio_clear_screen_vbe();
        return;
    }

    // run this is it's vga mode and not baller vbe mode
    // loop till every pixel on the display empty
	for (int y = 0; y < MAX_ROWS; y++) {
		for (int x = 0; x < MAX_COLS; x++) {
			dogeio_putchar(' ', x , y);
		}
	}

    // set the cursor position back to (0,0)
	cursor_x = 0;
    cursor_y = 0;
    dogeio_update_cursor(0, 0);
}

/*****************************************************
 * BASIC USER INPUT
 *****************************************************/

// get keyboard inputs when needed (like the input() function
// in python or fgets in C)
void dogeio_input(char* buffer, int max_len, uint8_t color) {
    // you still know... right?
    if (vbe_initialized) {
        dogeio_input_vbe(buffer, max_len, color);
        return;
    }

    // variables to check if shift is on
    // and i is 
    int i = 0;
    int shift = 0;
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;

    // wait until maximum length is reached or enter has been
    // hit
    while (i < max_len - 1) {
        // wait till port 0x64 and 0x01 gets an input
        while (!(ports_inb(0x64) & 0x01));
        uint8_t sc = ports_inb(0x60);

        if (sc == 0x2A || sc == 0x36) { shift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift = 0; continue; }
        if (sc & 0x80) continue;
        if (sc == 0x01) break;

        // hmmm memory issue
        if (sc >= sizeof(scan_to_ascii)) continue;
        char c = (shift) ? scan_to_ascii_shift[sc] : scan_to_ascii[sc];
        if (!c) continue;

        // this down here is the enter key
        if (c == '\n') {
            buffer[i] = '\0';
            cursor_x = 0;
            cursor_y++;
            return;

        // if the backspace key has been hit, then use \b to trail backwards
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                buffer[i] = '\0';
                if (cursor_x > 0) {
                    cursor_x--;
                } else if (cursor_y > 0) {
                    cursor_y--;
                    cursor_x = MAX_COLS - 1;
                }
                vga[cursor_y * MAX_COLS + cursor_x] = (uint16_t)' ' | (uint16_t)color << 8;
            }
        } else {
            buffer[i++] = c;
            vga[cursor_y * MAX_COLS + cursor_x] = (uint16_t)c | (uint16_t)color << 8;
            cursor_x++;

            if (cursor_x >= MAX_COLS) {
                cursor_x = 0;
                cursor_y++;
            }
        }

        // update the cursor position if the cursor is above 25, wait i realized
        // i can scroll
        if (cursor_y >= MAX_ROWS) {
            cursor_y = MAX_ROWS - 1;
            dogeio_scroll();
        }
        dogeio_update_cursor(cursor_x, cursor_y);
    }
    // print an null terminator
    buffer[i] = '\0';
}
