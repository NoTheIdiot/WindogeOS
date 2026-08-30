#ifndef BOOT_SYSCALL_H
#define BOOT_SYSCALL_H

#define DOGEIO_TEXT_EXIT 0
#define DOGEIO_TEXT_PRINT 1

#include <stdint.h>

static inline uint64_t _syscall(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    __asm__ volatile (
        "mov %1, %%rax\n\t"
        "mov %2, %%rdi\n\t"
        "syscall\n\t"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(num), "r"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#endif