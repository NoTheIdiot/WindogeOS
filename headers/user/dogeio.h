#ifndef USER_DOGEIO_H
#define USER_DOGEIO_H

#include <stdint.h>

static inline uint64_t syscall_0(uint64_t num) {
    uint64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall_1(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall_2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    uint64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall_3(uint64_t num, uint64_t arg1, uint64_t arg2,
                                 uint64_t arg3) {
    uint64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall_4(uint64_t num, uint64_t arg1, uint64_t arg2,
                                 uint64_t arg3, uint64_t arg4) {
    uint64_t ret;
    uint64_t syscall_arg4 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(syscall_arg4)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#define READ_FILE    01
#define WRITE_FILE   02
#define CREATE_FILE  03
#define DELETE_FILE  04
#define CREATE_DIR   05
#define RENAME_FILE  06
#define FILE_EXISTS  07
#define CHANGE_DIR   8
#define DELETE_LAST_LINE 9
#define COPY_FILE    10

#define PRINT        11
#define PRINTLN      12
#define CLEAR        13
#define INPUT        14
#define GET_KEY      15
#define PRINT_AT     16
#define TEXT_COLOR   17
#define BACKGROUND_COLOR 18

#define SYS_EXIT     60

static inline uint64_t syscall_read_file(const char *path, void *buffer, uint64_t size) {
    return syscall_3(READ_FILE, (uint64_t)path, (uint64_t)buffer, size);
}

static inline uint64_t syscall_write_file(const char *path, const char *text) {
    return syscall_2(WRITE_FILE, (uint64_t)path, (uint64_t)text);
}

static inline uint64_t syscall_input(const char *prompt, char *buffer, uint64_t capacity) {
    return syscall_3(INPUT, (uint64_t)prompt, (uint64_t)buffer, capacity);
}

static inline uint64_t syscall_print_at(const char *text, uint32_t x, uint32_t y,
                                        uint32_t color) {
    return syscall_4(PRINT_AT, (uint64_t)text, x, y, color);
}

static inline uint64_t syscall_text_color(uint32_t color) {
    return syscall_1(TEXT_COLOR, color);
}

static inline uint64_t syscall_background_color(uint32_t color) {
    return syscall_1(BACKGROUND_COLOR, color);
}

static inline uint64_t syscall_get_key(void) {
    return syscall_0(GET_KEY);
}

static inline uint64_t syscall_exit(uint64_t status) {
    return syscall_1(SYS_EXIT, status);
}

#endif