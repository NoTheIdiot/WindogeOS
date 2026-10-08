#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>

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
#define MKDIR_RECURSIVE    19
#define GET_TIME           20

#define SHELL              21
#define EXEC               22
#define STAT               23
#define APPEND_FILE        24
#define GET_CWD            25

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

#endif