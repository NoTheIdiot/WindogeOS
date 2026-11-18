#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define KERNEL_STACK_SIZE 8192
#define MAX_PROC_REGIONS  32
#define MAX_VMAS 16

typedef enum {
    PROC_RUNNING = 0,
    PROC_WAITING = 1,
    PROC_ZOMBIE  = 2
} proc_state_t;

typedef struct {
    uint64_t start;
    uint64_t end;
    uint64_t flags;             
} vm_region_t;

typedef struct {
    bool active;

    char filename[128];

    uint64_t p_vaddr;
    uint64_t p_memsz;

    uint64_t p_offset;
    uint64_t p_filesz;

    uint64_t pte_flags;
} vma_region_t;

typedef struct {
    uint64_t rip;
    uint64_t rsp;
    uint64_t rflags;
} user_context_t;

typedef struct process {
    uint64_t pid;

    proc_state_t state;

    uint64_t cr3;

    uint64_t entry_point;

    uint64_t argc;
    uint64_t argv;

    uint64_t saved_rsp;
    uint64_t saved_kernel_rsp;

    uint64_t kernel_stack_base;

    uint64_t heap_start;
    uint64_t heap_brk;

    uint64_t stack_top;

    vm_region_t regions[MAX_PROC_REGIONS];
    size_t region_count;

    vma_region_t vmas[MAX_VMAS];
    size_t vma_count;

    int child_exit_code;

    struct process *parent;
} process;

extern process *current_proc;

bool proc_create_stack(process* proc);

void proc_push64(
    process* proc,
    uint64_t value);

uint64_t proc_push_data(
    process* proc,
    const void* data,
    uint64_t size);

uint64_t proc_push_string(
    process* proc,
    const char* str);

uint64_t proc_build_argv(
    process* proc,
    int argc,
    char** argv);

bool proc_grow_stack(
    process* proc,
    uint64_t fault_addr);