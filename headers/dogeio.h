#ifndef DOGEIO_H
#define DOGEIO_H

// types
#include <stdint.h>
#include <stddef.h>

extern uint8_t terminal_font[128][16];

// good ol text stuff
void dogeio_text_putchar(char c, uint32_t x, uint32_t y);
void dogeio_text_clear();
void dogeio_text_printchar(char c);
void dogeio_text_print(const char *str);
void dogeio_text_println(const char *str);
void dogeio_text_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color);
void dogeio_text_input(const char* prompt, char* buffer, size_t max_string_length);
void dogeio_text_color_change(uint32_t color) ;

#endif