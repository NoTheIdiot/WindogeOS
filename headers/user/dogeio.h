#ifndef USER_DOGEIO_H
#define USER_DOGEIO_H

#include <stdint.h>

void syscall_0(uint64_t num) {
    __asm__ volatile (
        "syscall"
        :
        : "a"(num)
        : "rcx", "r11", "memory"
    );
}

void syscall_1(uint64_t num, uint64_t arg1) {
    __asm__ volatile (
        "syscall"
        :
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
}

#define SPECIAL 50
#define SYS_EXIT 60

#endif 