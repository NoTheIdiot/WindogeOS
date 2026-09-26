#include <stdint.h>
#include <string.h>
#include <boot/kernel.h>

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

static idt_entry_t idt[256];
static idt_ptr_t idtr;

// Change array declaration to allow accessing all 256 ISR stubs
extern void *isr_stub_table[];

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

    for (size_t i = 0; i < 256; i++) {
        set_idt_gate((uint8_t)i, (uint64_t)isr_stub_table[i], 0x08, 0x8E, 0);
    }

    idt[8].ist = 1;

    idtr.limit = sizeof(idt) - 1;
    idtr.base   = (uint64_t)&idt;

    idt_load(&idtr);
}