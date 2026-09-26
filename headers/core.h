#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stddef.h>

uint64_t allocate_flat_user_stack(uint64_t hhdm, uint64_t pml4_phys);

void init_syscall(void);
void init_user_space(void);
uint64_t pmm_alloc_block(void);
uint64_t core_c_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);
void core_to_user(uint64_t cr3_phys, void *entry_point, void *user_stack);
void init_syscalls(void);

#endif