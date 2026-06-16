#ifndef DOGEIO_GRAPHICS_H
#define DOGEIO_GRAPHICS_H

#include <stdint.h>
#include "multiboot.h"

void dogeio_init_vbe_from_multiboot(multiboot_info_t* mbi);
void dogeio_init_vbe();
void dogeio_clear_screen_vbe();
void dogeio_print_vbe(const char* string);
void dogeio_println_vbe(const char* string);
void dogeio_putchar_vbe(char c);
void dogeio_input_vbe(char* buffer, int max_len, uint8_t color);

#define WINDOGE_BG_COLOR 0x001a1a1a
#define WINDOGE_FG_COLOR 0x00e0e0e0
#define WINDOGE_PROMPT_COLOR 0x0000aa00

#endif
