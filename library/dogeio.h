#ifndef DOGEIO_H
#define DOGEIO_H

#include "stdint.h"
#include "stddef.h"
#include "root/syscalls.h"
#include "bool.h"

// types
typedef struct {
    char name[256];
    uint32_t size;
    bool is_dir;
} fs_t;

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

static inline void exit(void) {
    syscall_0(Process_Exit);
}

static inline int fs_read(const char *path, char *buf, size_t len) {
    return (int)syscall_3(ReadFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int fs_write(const char *path, const char *buf, size_t len) {
    return (int)syscall_3(WriteFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int fs_read_raw(const char *path, uint8_t *buf, size_t len) {
    return (int)syscall_3(ReadFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int fs_write_raw(const char *path, uint8_t *buf, size_t len) {
    return (int)syscall_3(WriteFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int fs_create(const char *path) {
    return (int)syscall_1(CreateFile, (uint64_t)path);
}

static inline int fs_mkdir(const char* path) {
    return (int)syscall_1(CreateDir, (uint64_t)path);
}

static inline int fs_delete(const char* path) {
    return (int)syscall_1(DeleteThing, (uint64_t)path);
}

static inline int fs_exists(const char* path) {
    return (int)syscall_1(Exists, (uint64_t)path);
}

static inline int fs_list(char* path, fs_t* buffer, int max) {
    return (int)syscall_3(ListDirectory, (uint64_t)path, (uint64_t)buffer, (uint64_t)max);
}

static inline int fs_chdir(char* path) {
    return (int)syscall_1(ChangeDirectory, (uint64_t)path);
}

static inline int fs_get_cwd(char *buf, size_t len) {
    return (int)syscall_2(CurrentDirectory, (uint64_t)buf, (uint64_t)len);
}

static inline int print(const char *str) {
    return (int)syscall_1(Print, (uint64_t)str);
}

static inline int text_color(uint32_t color) {
    return (int)syscall_1(TextColor, (uint64_t)color);
}

static inline int background_color(uint32_t color) {
    return (int)syscall_1(BackgroundColor, (uint64_t)color);
}

static inline int println(const char *str) {
    if (print(str) < 0) return -1;
    return print("\n");
}

static inline uint64_t print_at(const char *text, uint32_t x, uint32_t y,
                                uint32_t color) {
    return syscall_4(PrintAt, (uint64_t)text, x, y, color);
}

static inline void clear(void) {
    print("\033[2J\033[H");
}

static inline int input(const char *prompt, char *buf, size_t size) {
    return (int)syscall_3(Input, (uint64_t)prompt, (uint64_t)buf, (uint64_t)size);
}

static inline uint16_t get_key(void) {
    return (uint16_t)syscall_0(GetKey);
}

#endif
