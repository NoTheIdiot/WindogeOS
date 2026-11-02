#ifndef BOOT_SCHEDULER_H
#define BOOT_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>
#include <boot/task_info.h>

typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} interrupt_frame_t;

void scheduler_init(void);
interrupt_frame_t *scheduler_on_timer(interrupt_frame_t *frame);
int scheduler_spawn_elf(const char *path, int argc, char **argv,
                        uint32_t workspace_id, uint32_t *out_pid);
size_t scheduler_list(scheduler_task_info_t *tasks, size_t capacity);
int scheduler_kill(uint32_t pid);
uint32_t scheduler_current_pid(void);
uint32_t scheduler_current_workspace(void);
void scheduler_set_shell_workspace(uint32_t workspace_id);
void scheduler_exit_current(uint64_t status) __attribute__((noreturn));

void isr_resume_frame(interrupt_frame_t *frame) __attribute__((noreturn));

#endif
