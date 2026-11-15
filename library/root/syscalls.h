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
    __asm__ volatile (
        "mov %[a4], %%r10\n\t"
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), [a4] "r"(arg4)
        : "rcx", "r10", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_5(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    __asm__ volatile (
        "mov %[a4], %%r10\n\t"
        "mov %[a5], %%r8\n\t"
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), [a4] "r"(arg4), [a5] "r"(arg5)
        : "rcx", "r8", "r10", "r11", "memory"
    );
    return num;
}

// process management
#define Process_Exit 0

// file system operations
#define WriteFile   11
#define ReadFile    12
#define CreateFile  13
#define CreateDir   14
#define DeleteThing 15
#define Exists      16

// user input and output
#define Print 20
#define Input 21

#endif
