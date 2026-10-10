#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "../stdint.h"

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
#define READ_FILE_AT       26
#define MOVE_FILE          27
#define MAP_USER_MEMORY    28
#define PROTECT_USER_MEMORY 29
#define UNMAP_USER_MEMORY  30
#define GET_PID            31
#define GET_UID            32
#define GET_GID            33
#define GET_EUID           34
#define GET_EGID           35
#define GET_UNAME          36
#define RANDOM_BYTES       37
#define GET_USER_NAME      38
#define EXEC_ARGS          39
#define EXEC_ELF           40
#define LIST_DIR           41

#define USER_MEMORY_READ    0x1
#define USER_MEMORY_WRITE   0x2
#define USER_MEMORY_EXEC    0x4
#define USER_MEMORY_MAX_SIZE (16 * 1024 * 1024)

/* User memory calls use page-aligned addresses; map/protect also take permission flags. */
/* Native process IDs are currently single-process: PID is 1 and UID/GID are 0. */
/* RANDOM_BYTES takes (buffer, length, 0); it requires CPU RDRAND support. */
/* EXEC_ARGS takes (path, argc, argv); EXEC_ELF takes a path to an ELF file. */

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