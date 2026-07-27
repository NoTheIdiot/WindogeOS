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
void dogeio_text_input(const char* prompt, char* buffer, size_t max_string_length);
void dogeio_text_color_change(uint32_t color);
void dogeio_text_background_change(uint32_t color);
void dogeio_text_clear_raw();

// file system items
#define BLOCK_SIZE              512
#define MAX_FILES               64
#define MAX_FILENAME            32
#define TOTAL_DATA_BLOCKS       4096
#define BLOCKS_PER_FILE         64

struct fs_inode {
    uint8_t     used;
    char        filename[MAX_FILENAME];
    uint32_t    size;
    uint32_t    start_block;
};

struct fs_layout {
    uint32_t    magic;
    uint32_t    total_blocks;
    struct      fs_inode file_table[MAX_FILES];
    uint8_t     data_region[TOTAL_DATA_BLOCKS][BLOCK_SIZE];
};

#endif