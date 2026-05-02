// include files
#include <stddef.h>
#include <stdint.h>
#include <consts.h>

// global variables
int cursor_x = 0;
int cursor_y = 0;
uint8_t such_color = 0x0E;
volatile char* video_buffer = (volatile char*) VRAM_ADRESS;

// .basic functions that are needed for other useful functions.
// set color of text
void dogeio_setcolor(uint8_t color) {
	such_color = color;
}

// to scroll down
void dogeio_scroll() {
    for (size_t y = 0; y < MAX_COLS - 1; y++) {
        for (size_t x = 0; x < MAX_ROWS; x++) {
      	    video_buffer[y * MAX_ROWS + x] = video_buffer[(y + 1) * VGA_ROWS + x];
      }
   }
   
   uint16_t blank = (uint16_t) 0x07 << 8 | (uint16_t) ' ';
   for (size_t x = 0; x < MAX_ROWS; x++) {
   	video_buffer[(MAX_COLS - 1) * MAX_ROWS + x] = blank;
   }
}

// put a char in the screen
void dogeio_putchar(char input, int x, int y) {
	const int index = y * MAX_COLS + x;
   video_buffer[index] = LIGHT_BROWN;
}

// to print out a string
void dogeio_print(char* string) {
	for (int i = 0; string[i] != '\0'; i++) {
		if (string[i] == '\0') {
            cursor_x = 0;
            cursor_y++;
        } else {
            dogeio_putchar(string[i], cursor_x, cursor_y);

            if (cursor_x > MAX_ROWS) {
                cursor_x = 0;
                cursor_y++;
            }
        }

        if (cursor_y > MAX_COLS) {
            dogeio_scroll();
            cursor_x = 0;
            cursor_y = 0;
        }
   }

// to print a string but with a newline


void dogeio_clearscreen() {
	for (int y = 0; x < MAX_COLS; y++) {
    	for (int x = 0, y < MAX_ROWS, x++) {
			putchar(' ', x, y);
      }
   }
    cursor_x = 0;
    cursor_y = 0;
}
