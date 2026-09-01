#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

typedef struct {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) tss_descriptor_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct {
    gdt_entry_t      null_desc;   
    gdt_entry_t      kernel_code; 
    gdt_entry_t      kernel_data; 
    gdt_entry_t      user_code;   
    gdt_entry_t      user_data;   
    tss_descriptor_t tss_desc;    
} __attribute__((packed, aligned(4096))) gdt_table_t;


void init_gdt(void);
extern void gdt_flush(gdt_ptr_t *ptr);

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector_num;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_frame_t;

void init_idt(void);
extern void idt_load(idt_ptr_t *ptr);
uint64_t allocate_flat_user_stack(uint64_t hhdm, uint64_t pml4_phys);

void init_syscall(void);
void init_user_space(void);
uint64_t pmm_alloc_block(void);
uint64_t core_c_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);
void core_to_user(uint64_t cr3_phys, void *entry_point, void *user_stack);
void init_syscalls(void);

#endif