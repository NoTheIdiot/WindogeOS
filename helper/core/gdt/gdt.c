#include <stdint.h>
#include <string.h>
#include <core.h>

static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

static gdt_table_t gdt;
static tss_entry_t tss;
static gdt_ptr_t gdtr;

static void set_gdt_entry(gdt_entry_t *entry, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    entry->limit_low    = (uint16_t)(limit & 0xFFFF);
    entry->base_low     = (uint16_t)(base & 0xFFFF);
    entry->base_middle  = (uint8_t)((base >> 16) & 0xFF);
    entry->access       = access;
    entry->granularity  = (uint8_t)((limit >> 16) & 0x0F) | (gran & 0xF0);
    entry->base_high    = (uint8_t)((base >> 24) & 0xFF);
}

static void set_tss_descriptor(tss_descriptor_t *desc, uint64_t base, uint32_t limit) {
    desc->length     = (uint16_t)(limit & 0xFFFF);
    desc->base_low   = (uint16_t)(base & 0xFFFF);
    desc->base_mid   = (uint8_t)((base >> 16) & 0xFF);
    desc->flags1     = 0x89;
    desc->flags2     = (uint8_t)((limit >> 16) & 0x0F);
    desc->base_high  = (uint8_t)((base >> 24) & 0xFF);
    desc->base_upper = (uint32_t)(base >> 32);
    desc->reserved   = 0;
}

void init_gdt(void) {
    memset(&gdt, 0, sizeof(gdt));

    set_gdt_entry(&gdt.null_desc, 0, 0, 0, 0);
    set_gdt_entry(&gdt.kernel_code, 0, 0, 0x9A, 0x20);
    set_gdt_entry(&gdt.kernel_data, 0, 0, 0x92, 0x00);
    set_gdt_entry(&gdt.user_data,   0, 0, 0xF2, 0x00);
    set_gdt_entry(&gdt.user_code,   0, 0, 0xFA, 0x20);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)&gdt;

    gdt_flush(&gdtr);
}

void init_tss(void) {
    memset(&tss, 0, sizeof(tss));

    tss.rsp0 = (uint64_t)&kernel_stack[sizeof(kernel_stack)];
    tss.iomap_base = sizeof(tss_entry_t);

    set_tss_descriptor(&gdt.tss_desc, (uint64_t)&tss, sizeof(tss) - 1);

    __asm__ volatile ("ltr %0" :: "r"((uint16_t)0x28));
}