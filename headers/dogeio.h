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
int   fs_format(void);
int   fs_create(char* filename);
int   fs_mkdir(char* foldername);
int   fs_exists(char* filename);
int   fs_delete(char* filename);
int   fs_delete_last_line(char* filename);
int   fs_read(char* filename, char* output_buffer, uint32_t max_size);
int   fs_write(char* filename, char* input_buffer);
int   fs_list_dir(int hidden);
int   fs_rename(char* filename, char* newname);
void  fs_copy(char* source, char* dest);
int   fs_chdir(char* folder);
char* fs_dirname(void);

// ansi codes
#define COLOR_BLACK          0x000000
#define COLOR_RED            0xAA0000
#define COLOR_GREEN          0x00AA00
#define COLOR_YELLOW         0xAA5500
#define COLOR_BLUE           0x0000AA
#define COLOR_MAGENTA        0xAA00AA
#define COLOR_CYAN           0x00AAAA
#define COLOR_WHITE          0xAAAAAA

#define COLOR_BRIGHT_BLACK   0x555555
#define COLOR_BRIGHT_RED     0xFF5555
#define COLOR_BRIGHT_GREEN   0x55FF55
#define COLOR_BRIGHT_YELLOW  0xFFFF55
#define COLOR_BRIGHT_BLUE    0x5555FF
#define COLOR_BRIGHT_MAGENTA 0xFF55FF
#define COLOR_BRIGHT_CYAN    0x55FFFF
#define COLOR_BRIGHT_WHITE   0xFFFFFF



#endif