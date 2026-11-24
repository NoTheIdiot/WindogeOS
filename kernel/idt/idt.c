#include <stdint.h>
#include <stdbool.h>
#include <dogeio.h>

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_frame_t;

#define IDT_GATE_INT32 0x8E

static idt_entry_t idt[256];
static idtr_t      idtr;

extern uint64_t isr_stub_table[49];
extern void idt_load(idtr_t *idtr_ptr);

static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(val));
    return val;
}

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t ist, uint8_t flags) {
    idt[vector].offset_low      = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector        = 0x08;
    idt[vector].ist             = ist & 0x07;
    idt[vector].type_attributes = flags;
    idt[vector].offset_middle   = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high     = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved        = 0;
}

interrupt_frame_t *isr_common_handler(interrupt_frame_t *frame) {
    if (frame->vector == 255) {
        return frame;
    }

    if (frame->vector == 32) {
        apic_tick();
        return frame;
    }

    if (frame->vector == 14) {
        bool is_user = (frame->cs & 3) != 0;
        printf("\033[91m[Not Wow]\033[0m Page Fault (Vector 14)\n");
        printf("  Mode: %s\n", is_user ? "User (Ring 3)" : "Kernel (Ring 0)");

        sprintf("\033[91m[Not Wow]\033[0m Page Fault (Vector 14)\n");
        sprintf("  Mode: %s\n", is_user ? "User (Ring 3)" : "Kernel (Ring 0)");
    } else {
        switch (frame->vector) {
            case 0:  printf("\033[91m[Not Wow]\033[0m Divide-by-Zero (Vector 0)\n"); break;
            case 6:  printf("\033[91m[Not Wow]\033[0m Invalid Opcode (Vector 6)\n"); break;
            case 13: printf("\033[91m[Not Wow]\033[0m General Protection Fault (Vector 13)\n"); break;
            default: printf("\033[91m[Not Wow]\033[0m Unhandled Interrupt / Fault (Vector %lu)\n", frame->vector); break;
        }

        switch (frame->vector) {
            case 0:  sprintf("\033[91m[Not Wow]\033[0m Divide-by-Zero (Vector 0)\n"); break;
            case 6:  sprintf("\033[91m[Not Wow]\033[0m Invalid Opcode (Vector 6)\n"); break;
            case 13: sprintf("\033[91m[Not Wow]\033[0m General Protection Fault (Vector 13)\n"); break;
            default: sprintf("\033[91m[Not Wow]\033[0m Unhandled Interrupt / Fault (Vector %lu)\n", frame->vector); break;
        }
    }

    printf("  RIP: 0x%lx\n", frame->rip);
    printf("  ERR: 0x%lx\n", frame->error_code);

    if (frame->vector == 14) {
        printf("  CR2: 0x%lx\n", read_cr2());
        printf("  [PF Flags]:\n");
        printf("    - %s\n", !(frame->error_code & 1) ? "Not-Present page" : "Protection fault");
        printf("    - %s\n", (frame->error_code & 2) ? "Caused by a Write" : "Caused by a Read/Fetch");
        printf("    - %s\n", (frame->error_code & 4) ? "Access was User-mode" : "Access was Supervisor-mode");
        
        sprintf("  CR2: 0x%lx\n", read_cr2());
        sprintf("  [PF Flags]:\n");
        sprintf("    - %s\n", !(frame->error_code & 1) ? "Not-Present page" : "Protection fault");
        sprintf("    - %s\n", (frame->error_code & 2) ? "Caused by a Write" : "Caused by a Read/Fetch");
        sprintf("    - %s\n", (frame->error_code & 4) ? "Access was User-mode" : "Access was Supervisor-mode");
        if (frame->error_code & 16) {
            printf("    - Instruction Fetch\n");
            sprintf("    - Instruction Fetch\n");
        }
    }

    printf("  RSP: 0x%lx\n", frame->rsp);
    printf("  RFLAGS: 0x%lx\n", frame->rflags);

    sprintf("  RSP: 0x%lx\n", frame->rsp);
    sprintf("  RFLAGS: 0x%lx\n", frame->rflags);

    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

void init_idt(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, 0, 0, 0);
    }

    for (uint8_t vector = 0; vector < 32; vector++) {
        uint8_t ist_index = (vector == 8) ? 1 : 0;
        idt_set_gate(vector, isr_stub_table[vector], ist_index, IDT_GATE_INT32);
    }

    for (uint8_t vector = 32; vector < 48; vector++) {
        idt_set_gate(vector, isr_stub_table[vector], 0, IDT_GATE_INT32);
    }
    idt_set_gate(255, isr_stub_table[48], 0, IDT_GATE_INT32);

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    idt_load(&idtr);
}
