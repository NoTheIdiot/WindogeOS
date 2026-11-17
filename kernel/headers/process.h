#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>

#define KERNEL_STACK_SIZE 8192

typedef struct Process {
    uint64_t pid;
    uint64_t cr3;
    uint64_t saved_kernel_rsp;
    uint64_t kernel_stack_base;
    int child_exit_code;
    struct Process *parent;
} Process;

extern Process *current_proc;

#endif