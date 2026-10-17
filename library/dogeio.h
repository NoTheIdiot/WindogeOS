#ifndef USER_DOGEIO_H
#define USER_DOGEIO_H

#include "stdint.h"
#include "internal/syscalls.h"
#include <boot/syscall.h>

typedef struct {
    uint64_t size;
    uint64_t is_dir;
    uint64_t exists;
} dogec_stat_t;

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

#define KEY_UP        0x101
#define KEY_DOWN      0x102
#define KEY_LEFT      0x103
#define KEY_RIGHT     0x104
#define KEY_BACKSPACE 0x008
#define KEY_ENTER     0x00A
#define KEY_TAB       0x009
#define KEY_UNKNOWN   0x000
#define KEY_F1        0x105
#define KEY_F2        0x106
#define KEY_CTRL_O    0x110
#define KEY_CTRL_N    0x111


static inline uint64_t read_file(const char *path, void *buffer, uint64_t size) {
    return syscall_3(READ_FILE, (uint64_t)path, (uint64_t)buffer, size);
}

/* Raw reads return bytes without adding a string terminator. */
static inline uint64_t read_file_raw(const char *path, uint8_t *buffer,
                                     uint64_t size) {
    return syscall_3(READ_FILE, (uint64_t)path, (uint64_t)buffer, size);
}

static inline uint64_t read_file_at(const char *path, void *buffer,
                                    uint64_t offset, uint64_t size) {
    return syscall_4(READ_FILE_AT, (uint64_t)path, (uint64_t)buffer,
                     offset, size);
}

static inline uint64_t read_file_raw_at(const char *path, uint8_t *buffer,
                                        uint64_t offset, uint64_t size) {
    return syscall_4(READ_FILE_AT, (uint64_t)path, (uint64_t)buffer,
                     offset, size);
}

static inline uint64_t write_file(const char *path, const char *text) {
    return syscall_2(WRITE_FILE, (uint64_t)path, (uint64_t)text);
}

/* Raw writes replace the file with exactly size bytes. */
static inline uint64_t write_file_raw(const char *path, const uint8_t *buffer,
                                      uint64_t size) {
    return syscall_3(WRITE_FILE_RAW, (uint64_t)path, (uint64_t)buffer, size);
}

static inline uint64_t append_file(const char *path, const void *buffer,
                                   uint64_t size) {
    return syscall_3(APPEND_FILE, (uint64_t)path, (uint64_t)buffer, size);
}

static inline uint64_t create_file(const char *path) {
    return syscall_1(CREATE_FILE, (uint64_t)path);
}

static inline uint64_t delete_file(const char *path) {
    return syscall_1(DELETE_FILE, (uint64_t)path);
}

static inline uint64_t create_dir(const char *path) {
    return syscall_1(CREATE_DIR, (uint64_t)path);
}

static inline uint64_t mkdir_recursive(const char *path) {
    return syscall_1(MKDIR_RECURSIVE, (uint64_t)path);
}

static inline uint64_t get_time(uint64_t *out_time) {
    return syscall_1(GET_TIME, (uint64_t)out_time);
}

static inline uint64_t doge_uname(windoge_utsname_t *out_uname) {
    return syscall_1(GET_UNAME, (uint64_t)out_uname);
}

static inline uint64_t doge_random_bytes(void *buffer, uint64_t size) {
    return syscall_3(RANDOM_BYTES, (uint64_t)buffer, size, 0);
}

static inline uint64_t doge_get_user_name(char *buffer, uint64_t capacity) {
    return syscall_2(GET_USER_NAME, (uint64_t)buffer, capacity);
}

static inline uint64_t stat(const char *path, dogec_stat_t *out_stat) {
    return syscall_2(STAT, (uint64_t)path, (uint64_t)out_stat);
}

static inline uint64_t rename_file(const char *path, const char *new_path) {
    return syscall_2(RENAME_FILE, (uint64_t)path, (uint64_t)new_path);
}

static inline uint64_t file_exists(const char *path) {
    return syscall_1(FILE_EXISTS, (uint64_t)path);
}

static inline uint64_t change_dir(const char *path) {
    return syscall_1(CHANGE_DIR, (uint64_t)path);
}

static inline uint64_t get_cwd(char *buffer, uint64_t capacity) {
    return syscall_2(GET_CWD, (uint64_t)buffer, capacity);
}

static inline uint64_t list_dir(char (*names)[256], uint64_t capacity) {
    return syscall_2(LIST_DIR, (uint64_t)names, capacity);
}

static inline uint64_t delete_last_line(const char *path) {
    return syscall_1(DELETE_LAST_LINE, (uint64_t)path);
}

static inline uint64_t copy_file(const char *source, const char *destination) {
    return syscall_2(COPY_FILE, (uint64_t)source, (uint64_t)destination);
}

static inline uint64_t move_file(const char *source, const char *destination) {
    return syscall_2(MOVE_FILE, (uint64_t)source, (uint64_t)destination);
}

static inline uint64_t print(const char *text) {
    return syscall_1(PRINT, (uint64_t)text);
}

static inline uint64_t println(const char *text) {
    return syscall_1(PRINTLN, (uint64_t)text);
}

static inline uint64_t clear(void) {
    return syscall_0(CLEAR);
}

static inline uint64_t input(const char *prompt, char *buffer,
                             uint64_t capacity) {
    return syscall_3(INPUT, (uint64_t)prompt, (uint64_t)buffer, capacity);
}

static inline uint64_t get_key(void) {
    return syscall_0(GET_KEY);
}

static inline uint64_t print_at(const char *text, uint32_t x, uint32_t y,
                                uint32_t color) {
    return syscall_4(PRINT_AT, (uint64_t)text, x, y, color);
}

static inline uint64_t text_color(uint32_t color) {
    return syscall_1(TEXT_COLOR, color);
}

static inline uint64_t background_color(uint32_t color) {
    return syscall_1(BACKGROUND_COLOR, color);
}

static inline uint64_t sys_exit(uint64_t status) {
    return syscall_1(SYS_EXIT, status);
}

#endif
