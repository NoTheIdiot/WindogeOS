#ifndef DOGEIO_H
#define DOGEIO_H

#include <stdint.h>
#include <stddef.h>
#include "core.h"

extern uint8_t terminal_font[128][16];

#define TERMINAL_COLS 160
#define TERMINAL_ROWS 50

extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern uint32_t dogeio_background_color;
extern uint32_t dogeio_text_color;

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

#ifdef WINDOGE_APP

static inline void dogeio_text_putchar(char c, uint32_t x, uint32_t y) {
    core_syscall(DOGEIO_TEXT_PUTCHAR, (uint64_t)c, (uint64_t)x, (uint64_t)y, 0);
}
static inline void dogeio_text_clear(void) {
    core_syscall(DOGEIO_TEXT_CLEAR, 0, 0, 0, 0);
}
static inline void dogeio_text_printchar(char c) {
    core_syscall(DOGEIO_TEXT_PRINTCHAR, (uint64_t)c, 0, 0, 0);
}
static inline void dogeio_text_print(const char *str) {
    core_syscall(DOGEIO_TEXT_PRINT, (uint64_t)str, 0, 0, 0);
}
static inline void dogeio_text_println(const char *str) {
    core_syscall(DOGEIO_TEXT_PRINTLN, (uint64_t)str, 0, 0, 0);
}
static inline void dogeio_text_print_at(const char *str, uint32_t x, uint32_t y, uint32_t col) {
    core_syscall(DOGEIO_TEXT_PRINT_AT, (uint64_t)str, (uint64_t)x, (uint64_t)y, (uint64_t)col);
}
static inline void dogeio_text_input(const char* prompt, char* buffer, size_t max_len) {
    core_syscall(DOGEIO_TEXT_INPUT, (uint64_t)prompt, (uint64_t)buffer, (uint64_t)max_len, 0);
}
static inline void dogeio_text_color_change(uint32_t color) {
    core_syscall(DOGEIO_TEXT_COLOR_CHANGE, (uint64_t)color, 0, 0, 0);
}
static inline void dogeio_text_background_change(uint32_t color) {
    core_syscall(DOGEIO_TEXT_BACKGROUND_CHANGE, (uint64_t)color, 0, 0, 0);
}
static inline void dogeio_text_clear_raw(void) {
    core_syscall(DOGEIO_TEXT_CLEAR_RAW, 0, 0, 0, 0);
}

static inline int fs_format(void) { return (int)core_syscall(DOGEIO_FS_FORMAT, 0, 0, 0, 0); }
static inline int fs_create(char* file) { return (int)core_syscall(DOGEIO_FS_CREATE, (uint64_t)file, 0, 0, 0); }
static inline int fs_mkdir(char* dir) { return (int)core_syscall(DOGEIO_FS_MKDIR, (uint64_t)dir, 0, 0, 0); }
static inline int fs_exists(char* file) { return (int)core_syscall(DOGEIO_FS_EXISTS, (uint64_t)file, 0, 0, 0); }
static inline int fs_delete(char* file) { return (int)core_syscall(DOGEIO_FS_DELETE, (uint64_t)file, 0, 0, 0); }
static inline int fs_delete_last_line(char* file) { return (int)core_syscall(DOGEIO_FS_DELETE_LAST_LINE, (uint64_t)file, 0, 0, 0); }
static inline int fs_read(char* file, char* buf, uint32_t sz) { return (int)core_syscall(DOGEIO_FS_READ, (uint64_t)file, (uint64_t)buf, (uint64_t)sz, 0); }
static inline int fs_write(char* file, char* buf) { return (int)core_syscall(DOGEIO_FS_WRITE, (uint64_t)file, (uint64_t)buf, 0, 0); }
static inline int fs_list_dir(int hidden) { return (int)core_syscall(DOGEIO_FS_LIST_DIR, (uint64_t)hidden, 0, 0, 0); }
static inline int fs_rename(char* oldn, char* newn) { return (int)core_syscall(DOGEIO_FS_RENAME, (uint64_t)oldn, (uint64_t)newn, 0, 0); }
static inline void fs_copy(char* src, char* dst) { core_syscall(DOGEIO_FS_COPY, (uint64_t)src, (uint64_t)dst, 0, 0); }
static inline int fs_chdir(char* dir) { return (int)core_syscall(DOGEIO_FS_CHDIR, (uint64_t)dir, 0, 0, 0); }
static inline char* fs_dirname(void) { return (char*)core_syscall(DOGEIO_FS_DIRNAME, 0, 0, 0, 0); }
static inline int fs_mount(void) { return (int)core_syscall(DOGEIO_FS_MOUNT, 0, 0, 0, 0); }

static inline int exec_flat_binary(const char *file, int argc, char **argv) {
    return (int)core_syscall(DOGEIO_EXEC_FLAT_BINARY, (uint64_t)file, (uint64_t)argc, (uint64_t)argv, 0);
}

#else

void dogeio_text_putchar(char c, uint32_t x, uint32_t y);
void dogeio_text_clear(void);
void dogeio_text_printchar(char c);
void dogeio_text_print(const char *str);
void dogeio_text_println(const char *str);
void dogeio_text_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color);
void dogeio_text_input(const char* prompt, char* buffer, size_t max_str_length);
void dogeio_text_color_change(uint32_t color);
void dogeio_text_background_change(uint32_t color);
void dogeio_text_clear_raw(void);

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
int   fs_mount(void);

int exec_flat_binary(const char *filename, int argc, char **argv);

#endif
#endif