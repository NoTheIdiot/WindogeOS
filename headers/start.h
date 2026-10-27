#ifndef START_H
#define START_H

#include <stdint.h>

void start_shell(void);
void start_setup(void);

void init_gdt_tss(void);
void init_idt(void);
void init_syscalls(void);
void init_pit(uint32_t target_frequency);

#endif