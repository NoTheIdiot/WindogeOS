#ifndef DOGEIO_H
#define DOGEIO_H

#include <stdint.h>
#include <stddef.h>

extern const uint8_t terminal_font[256][16];
void dogeio_text_putchar(char c, uint32_t x_pos, uint32_t y_pos);
void dogeio_text_print(const char *str);
void dogeio_text_println(const char* str);
void dogeio_text_clear();

// inputs
char dogeio_text_getchar(void);
void dogeio_text_input(const char *prompt, char *buffer, size_t max_len);

#endif