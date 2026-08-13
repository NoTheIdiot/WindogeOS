#ifndef DOGEIO_H
#define DOGEIO_H

// types
#include <stdint.h>
#include <stddef.h>

extern uint8_t terminal_font[128][16];

#define TERMINAL_COLS 160
#define TERMINAL_ROWS 50

extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern uint32_t dogeio_background_color;
extern uint32_t dogeio_text_color;

// good ol text stuff
void dogeio_text_putchar(char c, uint32_t x, uint32_t y);
void dogeio_text_clear();
void dogeio_text_printchar(char c);
void dogeio_text_print(const char *str);
void dogeio_text_println(const char *str);
void dogeio_text_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color);
void dogeio_text_input(const char* prompt, char* buffer, size_t max_str_length);
void dogeio_text_color_change(uint32_t color);
void dogeio_text_background_change(uint32_t color);
void dogeio_text_clear_raw();

// file system stuff
int fs_format(void);
int fs_create(char* filename);
int fs_delete(char* filename);
int fs_delete_last_line(char* filename);
int fs_read(char* filename, char* output_buffer, uint32_t max_size);
int fs_write(char* filename, char* input_buffer);
int fs_exists(char* filename);
void fs_copy(char* source, char* dest);
int fs_list_dir(int show_hidden);
int fs_rename(char* filename, char* newname);

#endif