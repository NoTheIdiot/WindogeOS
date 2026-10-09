#ifndef BOOT_SYSCALL_INTERNAL_H
#define BOOT_SYSCALL_INTERNAL_H

#include <bool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t user_rsp;
} __attribute__((packed)) syscall_registers_t;

bool user_page_accessible(uint64_t address, bool write_access);
bool user_range_accessible(uint64_t address, uint64_t length, bool write_access);
bool copy_user_string(uint64_t address, char *buffer, size_t capacity,
                      size_t *string_length);
bool user_string_length(uint64_t address, size_t capacity, size_t *string_length);
uint64_t linux_syscall_handler(syscall_registers_t *regs);
void linux_syscall_reset(void);
void linux_syscall_set_heap_base(uint64_t address);
extern uint64_t kernel_program_launcher_rsp;
void syscall_exit_to_launcher(uint64_t exit_code) __attribute__((noreturn));
void syscall_set_linux_abi(bool enabled);

#endif
