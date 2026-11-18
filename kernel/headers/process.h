#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define KERNEL_STACK_SIZE 8192
#define MAX_PROC_REGIONS  32

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

typedef struct Process {
    uint64_t pid;
    proc_state_t state;
    
    uint64_t cr3;               
    uint64_t saved_kernel_rsp;  
    uint64_t kernel_stack_base; 

    uint64_t heap_start;        
    uint64_t heap_brk;          
    uint64_t stack_top;         
    vm_region_t regions[MAX_PROC_REGIONS];
    size_t region_count;
    
    int child_exit_code;        
    struct Process *parent;     
} Process;

extern Process *current_proc;

void proc_init_root(uint64_t root_cr3, uint64_t root_kstack_base);
int  spawn(const char *user_path, const char *const *user_argv);
__attribute__((noreturn)) void exit(int code);
