#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <bool.h>

// some idt gdt and tss
void init_gdt_tss(void);
void init_idt(void);
extern void idt_load(idt_ptr_t *ptr);

#endif