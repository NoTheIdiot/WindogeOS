#ifndef USER_SYSCALLS_H
#define USER_SYSCALLS_H

#include "stdint.h"

static inline uint64_t syscall_0(uint64_t num) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : 
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_1(uint64_t num, uint64_t arg1) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_4(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    register uint64_t r10 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_5(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8  __asm__("r8")  = arg5;
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return num;
}

#define Process_Exit 0
#define Print 1

#endif