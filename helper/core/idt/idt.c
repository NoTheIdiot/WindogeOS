#include <stdint.h>
#include <stddef.h>
#include <boot/kernel.h>
#include <dogeio.h>
#include <string.h>
#include <basicutil.h>

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

typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

#define IDT_GATE_INT32 0x8E 

static idt_entry_t idt[256];
static idtr_t      idtr;

extern uint64_t isr_stub_table[32];
extern void idt_load(idtr_t *idtr_ptr);

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t ist, uint8_t flags) {
    idt[vector].offset_low      = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector        = 0x08;               
    idt[vector].ist             = ist & 0x07;          
    idt[vector].type_attributes = flags;
    idt[vector].offset_middle   = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high     = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved        = 0;
}

void isr_common_handler(interrupt_frame_t *frame) {
    char hex_buf[32];

    dogeio_text_print("\n--- PANIC: EXCEPTION ");
    uint64_to_str(frame->vector, hex_buf);
    dogeio_text_print(hex_buf);
    dogeio_text_println(" ---");

    dogeio_text_print("RIP: 0x");
    uint64_to_str(frame->rip, hex_buf);
    dogeio_text_println(hex_buf);

    dogeio_text_print("ERR: 0x");
    uint64_to_str(frame->error_code, hex_buf);
    dogeio_text_println(hex_buf);

    if (frame->vector == 14) {
        uint64_t cr2;
        __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
        dogeio_text_print("CR2: 0x");
        uint64_to_str(cr2, hex_buf);
        dogeio_text_println(hex_buf);
    }
    
    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

void init_idt(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, 0, 0, 0);
    }
    
    for (uint8_t vector = 0; vector < 32; vector++) {
        uint8_t ist_index = 0;
        if (vector == 8) {
            ist_index = 1;
        }
        idt_set_gate(vector, isr_stub_table[vector], ist_index, IDT_GATE_INT32);
    }
    
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;
    
    idt_load(&idtr);
}
