#ifndef DOGEIO_H
#define DOGEIO_H

#include <stdint.h>
#include <stddef.h>
#include <bool.h>

#define TERMINAL_COLS 160
#define TERMINAL_ROWS 50

#define COLOR_BACKGROUND            0x0C0C0C
#define COLOR_DARK_GRAY             0x404040
#define COLOR_GRAY                  0xCCCCCC
#define COLOR_BRIGHT_GRAY           0xE0E0E0
#define COLOR_CURSOR                0xFFFFFF

#define COLOR_DARK_BLACK            0x000000
#define COLOR_DARK_BLUE             0x001F82
#define COLOR_DARK_GREEN            0x0B5E08
#define COLOR_DARK_CYAN             0x21547A
#define COLOR_DARK_RED              0x6F0912
#define COLOR_DARK_PURPLE           0x4D0D56
#define COLOR_DARK_MAGENTA          0x4D0D56
#define COLOR_DARK_ORANGE           0x803700
#define COLOR_DARK_YELLOW           0x6E5900
#define COLOR_DARK_WHITE            0x999999

#define COLOR_BLACK                 0x0C0C0C
#define COLOR_BLUE                  0x0037DA
#define COLOR_GREEN                 0x13A10E
#define COLOR_CYAN                  0x3A96DD
#define COLOR_RED                   0xC50F1F
#define COLOR_PURPLE                0x881798
#define COLOR_MAGENTA               0x881798
#define COLOR_ORANGE                0xD85D00
#define COLOR_YELLOW                0xC19C00
#define COLOR_WHITE                 0xF0F0F0

#define COLOR_BRIGHT_BLACK          0x767676
#define COLOR_BRIGHT_BLUE           0x3B78FF
#define COLOR_BRIGHT_GREEN          0x16C60C
#define COLOR_BRIGHT_CYAN           0x61D6D6
#define COLOR_BRIGHT_RED            0xE74856
#define COLOR_BRIGHT_PURPLE         0xB4009E
#define COLOR_BRIGHT_MAGENTA        0xB4009E
#define COLOR_BRIGHT_ORANGE         0xFF8C3A
#define COLOR_BRIGHT_YELLOW         0xF9F1A5
#define COLOR_BRIGHT_WHITE          0xFFFFFF

extern char text_grid[TERMINAL_ROWS * TERMINAL_COLS];
extern uint32_t text_color_grid[TERMINAL_ROWS * TERMINAL_COLS];
extern uint32_t bg_color_grid[TERMINAL_ROWS * TERMINAL_COLS];

extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern bool     cursor_on;
extern uint32_t background_color;
extern uint32_t text_color;

typedef struct {
    char name[256];
    uint32_t size;
    bool is_dir;
} fs_t;

void putchar(char c, uint32_t x, uint32_t y);
void printchar(char c);
void print(char* text);
void println(char* text);
void clear();
void cursor_show(void);
void cursor_hide(void);

uint16_t get_key(void);
void input(char* prompt, char* buffer);

void sprint(const char *str);
void sprintln(const char *str);

int fs_mount(void);
int fs_create(char* filename);
int fs_read(char* filename, char* data, uint64_t size);
int fs_write(char* filename, char* data, uint64_t size);
bool fs_exists(char *filename);
int fs_write_force(char* filename, char* data, uint64_t size);
int fs_list(fs_t* out, int max_entries);
int fs_delete(char* filename);
int fs_read_raw(char *filename, uint8_t *buffer, uint64_t size);
int fs_write_raw(char* filename, uint8_t* buffer, uint64_t size);
int fs_read_raw_at(char *filename, uint8_t *buffer, uint64_t offset, uint64_t size);

void update_time(void);
uint64_t get_epoch_seconds(void);
char* get_time(void);
char* get_date(void);

int run_elf(char *filename, uint64_t size);

#endif