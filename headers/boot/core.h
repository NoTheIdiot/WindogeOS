#ifndef CORE_INTERNAL_H
#define CORE_INTERNAL_H

#include <stdint.h>

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
    gdt_entry_t      null_desc;   // 0x00
    gdt_entry_t      kernel_code; // 0x08
    gdt_entry_t      kernel_data; // 0x10
    gdt_entry_t      user_data;   // 0x18
    gdt_entry_t      user_code;   // 0x20
    tss_descriptor_t tss_desc;    // 0x28
} __attribute__((packed, aligned(4096))) gdt_table_t;

void init_gdt(void);
extern void gdt_flush(gdt_ptr_t *ptr);

#endif