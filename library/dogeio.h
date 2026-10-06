#ifndef USER_DOGEIO_H
#define USER_DOGEIO_H

#include <stdint.h>
#include "string.h"

#define READ_FILE          1
#define WRITE_FILE         2
#define CREATE_FILE        3
#define DELETE_FILE        4
#define CREATE_DIR         5
#define RENAME_FILE        6
#define FILE_EXISTS        7
#define CHANGE_DIR         8
#define DELETE_LAST_LINE   9
#define COPY_FILE          10

#define PRINT              11
#define PRINTLN            12
#define CLEAR              13
#define INPUT              14
#define GET_KEY            15
#define PRINT_AT           16
#define TEXT_COLOR         17
#define BACKGROUND_COLOR   18

#define SYS_EXIT           60

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
    register uint64_t syscall_arg4 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(syscall_arg4)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t read_file(const char *path, void *buffer, uint64_t size) {
    return syscall_3(READ_FILE, (uint64_t)path, (uint64_t)buffer, size);
}

static inline uint64_t write_file(const char *path, const char *text) {
    return syscall_2(WRITE_FILE, (uint64_t)path, (uint64_t)text);
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

static inline uint64_t rename_file(const char *path, const char *new_path) {
    return syscall_2(RENAME_FILE, (uint64_t)path, (uint64_t)new_path);
}

static inline uint64_t file_exists(const char *path) {
    return syscall_1(FILE_EXISTS, (uint64_t)path);
}

static inline uint64_t change_dir(const char *path) {
    return syscall_1(CHANGE_DIR, (uint64_t)path);
}

static inline uint64_t delete_last_line(const char *path) {
    return syscall_1(DELETE_LAST_LINE, (uint64_t)path);
}

static inline uint64_t copy_file(const char *source, const char *destination) {
    return syscall_2(COPY_FILE, (uint64_t)source, (uint64_t)destination);
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
