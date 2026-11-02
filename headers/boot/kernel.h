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
void sys_workspace_save(uint32_t workspace_id);
void sys_workspace_load(uint32_t workspace_id, bool render);
void sys_workspace_request_switch(void);
bool sys_workspace_take_switch_request(void);
uint32_t sys_workspace_generation(void);
bool pit_take_status_bar_refresh(void);

// init sum gdt tss and idt
// though i equally suck at all of these ta
void init_gdt_tss(void);
void init_idt(void);
void init_pit(uint32_t target_frequency);
void pit_handler(void);

extern bool is_kernel_dead;

#endif