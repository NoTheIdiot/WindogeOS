#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <bool.h>

// useless visual only things
// though it's not really useless.....
#define MAX_TERMINALS 4
extern int current_terminal_id;

void menubar_draw(void);
void sys_switch_terminal(void);
void sys_init_terminals(void);

// init sum gdt tss and idt
// though i equally suck at all of these ta
void init_gdt_tss(void);
void init_idt(void);

extern bool is_kernel_dead;

#endif