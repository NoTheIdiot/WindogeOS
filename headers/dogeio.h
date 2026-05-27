#ifndef DOGEIO_H
#define DOGEIO_H

#include "multiboot.h"

// scancode
extern char scan_to_ascii[];
extern char scan_to_ascii_shift[];

void dogeio_putchar(char input);
void dogeio_print(char* string);
void dogeio_println(char* string); 
void dogeio_clear_screen();
void dogeio_update_cursor(int x, int y);
void dogeio_input(char* buffer, int max_len, uint8_t color);
void dogeio_get_cpu_name(char* buffer);
uint32_t dogeio_get_total_ram(multiboot_info_t* mbi);

#endif
