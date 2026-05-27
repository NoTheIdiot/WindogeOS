#ifndef DOGEIO_GRAPHICS_H
#define DOGEIO_GRAPHICS_H

#include <stdint.h>
#include "../kernel/multiboot.h"

void dogeio_init_graphics_from_multiboot(multiboot_info_t* mbi);
void dogeio_init_graphics();
void dogeio_clear_screen_graphics();
void dogeio_print_graphics(const char* string);
void dogeio_println_graphics(const char* string);
void dogeio_putchar_graphics(char c);
void dogeio_input_graphics(char* buffer, int max_len, uint8_t color);

#define WINDOGE_BG_COLOR 0x001a1a1a
#define WINDOGE_FG_COLOR 0x00e0e0e0
#define WINDOGE_PROMPT_COLOR 0x0000aa00

#endif
