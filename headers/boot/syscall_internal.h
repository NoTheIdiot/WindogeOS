#ifndef BOOT_SYSCALL_INTERNAL_H
#define BOOT_SYSCALL_INTERNAL_H

#include <bool.h>
#include <stddef.h>
#include <stdint.h>

#define LINUX_TASK_FD_LIMIT 32
#define LINUX_TASK_PATH_LIMIT 256
#define LINUX_TASK_CONTEXT_SIZE 16384

typedef struct {
    bool open;
    bool readable;
    bool writable;
    bool append;
    bool is_dir;
    bool is_terminal;
    uint64_t offset;
    char path[LINUX_TASK_PATH_LIMIT];
} linux_task_fd_t;

typedef struct {
    linux_task_fd_t file_descriptors[LINUX_TASK_FD_LIMIT];
    uint64_t program_break;
    uint64_t mmap_cursor;
    uint64_t heap_start;
    uint8_t reserved[LINUX_TASK_CONTEXT_SIZE -
                     sizeof(linux_task_fd_t) * LINUX_TASK_FD_LIMIT -
                     sizeof(uint64_t) * 3];
} linux_task_context_t;

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

typedef struct {
    uint64_t* kernel_rsp;
    uint32_t pid;
    uint32_t status; 
    uint64_t page_directory;
    struct process* next;
} process_t;

bool user_page_accessible(uint64_t address, bool write_access);
bool user_range_accessible(uint64_t address, uint64_t length, bool write_access);
bool copy_user_string(uint64_t address, char *buffer, size_t capacity,
                      size_t *string_length);
bool user_string_length(uint64_t address, size_t capacity, size_t *string_length);
uint64_t linux_syscall_handler(syscall_registers_t *regs);
void linux_syscall_reset(void);
void linux_syscall_set_heap_base(uint64_t address);
void linux_syscall_context_save(linux_task_context_t *context);
void linux_syscall_context_restore(const linux_task_context_t *context);
void linux_syscall_context_init(linux_task_context_t *context);
extern uint64_t kernel_program_launcher_rsp;
void syscall_exit_to_launcher(uint64_t exit_code) __attribute__((noreturn));
void syscall_set_linux_abi(bool enabled);
void syscall_select_linux_abi(bool enabled);
bool syscall_linux_abi_active(void);
uint64_t syscall_user_kernel_gs_base(void);

#endif
