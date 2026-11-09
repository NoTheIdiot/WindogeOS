#include <stdint.h>
#include <dogeio.h>
#include <system.h>
#include <bool.h>

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
#define COM1_PORT 0x3F8

uint8_t isr_fpu_temp[512] __attribute__((aligned(16)));
static idt_entry_t idt[256];
static idtr_t      idtr;

extern uint64_t isr_stub_table[49];
extern void idt_load(idtr_t *idtr_ptr);

static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(val));
    return val;
}

static void uint64_to_hex(uint64_t val, char *buf) {
    const char *hex = "0123456789abcdef";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buf[2 + i] = hex[(val >> ((15 - i) * 4)) & 0xF];
    }
    buf[18] = '\0';
}

static void append_str(char *dest, const char *src, int *idx) {
    while (*src) {
        dest[(*idx)++] = *src++;
    }
    dest[*idx] = '\0';
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

    switch (frame->vector) {
        case 0:  sprintln("[Not Wow] Divide-by-Zero (Vector 0)"); break;
        case 6:  sprintln("[Not Wow] Invalid Opcode (Vector 6)"); break;
        case 13: sprintln("[Not Wow] General Protection Fault (Vector 13)"); break;
        case 14: sprintln("[Not Wow] Page Fault (Vector 14)"); break;
        default: sprintln("[Not Wow] Unhandled Interrupt / Fault"); break;
    }

    char buf[64];
    int idx;
    char hex_str[19];

    idx = 0;
    uint64_to_hex(frame->rip, hex_str);
    append_str(buf, "  RIP: ", &idx);
    append_str(buf, hex_str, &idx);
    sprintln(buf);

    idx = 0;
    uint64_to_hex(frame->error_code, hex_str);
    append_str(buf, "  ERR: ", &idx);
    append_str(buf, hex_str, &idx);
    sprintln(buf);

    if (frame->vector == 14) {
        idx = 0;
        uint64_to_hex(read_cr2(), hex_str);
        append_str(buf, "  CR2: ", &idx);
        append_str(buf, hex_str, &idx);
        sprintln(buf);
    }

    idx = 0;
    uint64_to_hex(frame->rsp, hex_str);
    append_str(buf, "  RSP: ", &idx);
    append_str(buf, hex_str, &idx);
    sprintln(buf);

    idx = 0;
    uint64_to_hex(frame->rflags, hex_str);
    append_str(buf, "  RFLAGS: ", &idx);
    append_str(buf, hex_str, &idx);
    sprintln(buf);

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