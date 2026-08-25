#include <stdint.h>
#include <string.h>
#include <core.h>

static idt_entry_t idt[256];
static idt_ptr_t idtr;

extern void *isr_stub_table[32];

static void set_idt_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist) {
    idt[num].offset_low      = (uint16_t)(base & 0xFFFF);
    idt[num].selector        = sel;
    idt[num].ist             = ist;
    idt[num].type_attributes = flags;
    idt[num].offset_mid      = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high     = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].zero            = 0;
}

void exception_handler(interrupt_frame_t *frame) {
    (void)frame;
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

void init_idt(void) {
    memset(idt, 0, sizeof(idt));

    for (uint8_t i = 0; i < 32; i++) {
        set_idt_gate(i, (uint64_t)isr_stub_table[i], 0x08, 0x8E, 0);
    }

    idt[8].ist = 1;

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    idt_load(&idtr);
}