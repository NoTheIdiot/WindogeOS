#include <boot/kernel.h>
#include <stdint.h>
#include <stddef.h>

// gdt type struct
// also why __attribute__((packed)) after typedef strcut?
typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} gdt_entry_t;

// extended gdt type struct (tss)
typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} tss_entry_t;

typedef struct __attribute__((packed)) {
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
} tss_t;

// actual structs used
typedef struct __attribute__((packed)) {
    gdt_entry_t null_desc;    // 0x00
    gdt_entry_t kernel_code;  // 0x08
    gdt_entry_t kernel_data;  // 0x10
    gdt_entry_t user_data;    // 0x18
    gdt_entry_t user_code;    // 0x20
    tss_entry_t tss_desc;     // 0x28
} gdt_t;
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} gdtr_t;

// 16kb stacks
static uint8_t kernel_stack[16384] __attribute__((aligned(16)));
static uint8_t double_fault_stack[16384] __attribute__((aligned(16)));

gdt_t gdt;
tss_t tss;
gdtr_t gdtr;

extern void gdt_flush(gdtr_t* gdtr_ptr);

void gdt_set_entry(gdt_entry_t *entry, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    entry->base_low     = (uint16_t)(base & 0xFFFF);
    entry->base_middle  = (uint8_t)((base >> 16) & 0xFF);
    entry->base_high    = (uint8_t)((base >> 24) & 0xFF);
    entry->limit_low    = (uint16_t)(limit & 0xFFFFF);
    entry->granularity  = (uint8_t)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    entry->access       = access;
}

void gdt_set_tss_entry(tss_entry_t *entry, uint64_t base, uint32_t limit) {
    entry->base_low    = (uint16_t)(base & 0xFFFF);
    entry->base_middle = (uint8_t)((base >> 16) & 0xFF);
    entry->base_high   = (uint8_t)((base >> 24) & 0xFF);
    entry->base_upper  = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    entry->limit_low   = (uint16_t)(limit & 0xFFFF);
    entry->granularity = (uint8_t)((limit >> 16) & 0x0F);
    entry->access      = 0x89;
    entry->reserved    = 0;
}

void init_gdt_tss(void) {
    uint8_t *tss_ptr = (uint8_t *)&tss;
    for (size_t i = 0; i < sizeof(tss_t); i++) {
        tss_ptr[i] = 0;
    }

    tss.rsp0 = (uint64_t)&kernel_stack[sizeof(kernel_stack)];
    tss.ist1 = (uint64_t)&double_fault_stack[sizeof(double_fault_stack)];

    gdt_set_entry(&gdt.null_desc, 0, 0, 0, 0);
    gdt_set_entry(&gdt.kernel_code, 0, 0xFFFFFFFF, 0x9A, 0xA0);
    gdt_set_entry(&gdt.kernel_data, 0, 0xFFFFFFFF, 0x92, 0xC0);
    gdt_set_entry(&gdt.user_data, 0, 0xFFFFFFFF, 0xF2, 0xC0);
    gdt_set_entry(&gdt.user_code, 0, 0xFFFFFFFF, 0xFA, 0xA0);
    gdt_set_tss_entry(&gdt.tss_desc, (uint64_t)&tss, sizeof(tss_t) - 1);
    gdtr.limit = sizeof(gdt_t) - 1;
    gdtr.base  = (uint64_t)&gdt;
    gdt_flush(&gdtr);
}