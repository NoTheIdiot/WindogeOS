#ifndef USER_SYSCALLS_H
#define USER_SYSCALLS_H

#include "stdint.h"

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
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall_5(uint64_t num, uint64_t arg1, uint64_t arg2,
                               uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    uint64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4), "r"(arg5)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#define Process_Exit 0
#define Print 1

#endif