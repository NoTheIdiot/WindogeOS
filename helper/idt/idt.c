#include <bool.h>
#include <stdint.h>
#include <drivers.h>

// macros
// if you are wondering why didn't i use variables instead,
// macros are just replacements, variables take up ram.
#define KEY_BUFFER_SIZE 256

// alright this needs to be changed so NO MACROS IDIOT
static volatile uint8_t kbd_buffer[KEY_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

// extern other funcitons from the idt stub cuz C is 
// actually assembly, also wondering what is (void) in function
// args????
extern void irq_stub(void);

// allocate space for all 256 arch vectors
static idt_entry_t idt[256];

// Allocate the tracking structure that holds the pointer/limit for the CPU
static idtr_t idtr;

// a irq handler
void irq_handler(void) {
    // Read the scan code from the PS/2 data port immediately
    uint8_t code = lowlevel_ports_inb(0x60);
    
    // Non-blocking ring buffer push logic
    int next = (kbd_head + 1) % KEY_BUFFER_SIZE;
    if (next != kbd_tail) { 
        kbd_buffer[kbd_head] = code;
        kbd_head = next;
    }

    // Always send End of Interrupt (EOI)
    lowlevel_ports_outb(0x20, 0x20);
}

void drivers_idt_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t flags) {
    // wonder how C lets you do this but it's better and cleaner
    idt[vector].offset_1    = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = flags;
    idt[vector].offset_2    = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_3    = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

void drivers_idt_pic_remap(void) {
    lowlevel_ports_outb(0x20, 0x11); 
    lowlevel_ports_outb(0xA0, 0x11);
    lowlevel_ports_outb(0x21, 0x20);
    lowlevel_ports_outb(0xA1, 0x28);
    lowlevel_ports_outb(0x21, 0x04); // Master configuration pin
    lowlevel_ports_outb(0xA1, 0x02); // Slave configuration pin
    lowlevel_ports_outb(0x21, 0x01); // 8086 Environment Mode
    lowlevel_ports_outb(0xA1, 0x01); // 8086 Environment Mode

    // Unmask only IRQ1 (Keyboard) on Master, mask everything else
    lowlevel_ports_outb(0x21, 0xFD);
    lowlevel_ports_outb(0xA1, 0xFF); 
}

void idt_init(void) {
    while (lowlevel_ports_inb(0x64) & 1) {
        lowlevel_ports_inb(0x60);
    }
    drivers_idt_pic_remap();
    drivers_idt_gate(0x21, (uint64_t)irq_stub, 0x08, 0x8E);
    
    // Filled out the idtr structure fields instead of targeting the raw array directly
    idtr.limit = (uint16_t)(sizeof(idt_entry_t) * 256) - 1;
    idtr.base  = (uint64_t)&idt;
    
    // Passed the idtr tracker to the lidt instruction
    __asm__ volatile ("lidt %0" : : "m"(idtr));
    __asm__ volatile ("sti");
}
