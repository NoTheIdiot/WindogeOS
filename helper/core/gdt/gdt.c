#include <boot/kernel.h>
#include <stdint.h>
#include <stddef.h>

// gdt type struct
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} gdt_entry_t __attribute__((packed));

// extended gdt type struct (tss)
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} tss_entry_t __attribute__((packed));

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
} tss_t __attribute__((packed));

// actual structs used
typedef struct {
    gdt_entry_t null_desc;
    gdt_entry_t kernel_code;
    gdt_entry_t kernel_data;
    gdt_entry_t user_data;
    gdt_entry_t user_code;
    tss_entry_t tss_desc;
} __attribute__((packed)) gdt_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

// 16kb stack
static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

gdt_t gdt;
tss_t tss;
gdtr_t gdtr;

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
    tss.iomap_base = sizeof(tss_t);

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