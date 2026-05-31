#include <stdint.h>
#include "idt.h"
#include "ports.h"
#include "time.h"

struct IDTEntry idt[256];
struct IDTPointer idtp;

void load_idt(struct IDTPointer* idtp) {
    __asm__ volatile("lidt (%0)" : : "r"(idtp));
}

void set_idt_gate(int n, uint32_t handler) {
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].selector = 0x08;
    idt[n].zero = 0;
    idt[n].type_attr = 0x8E;
    idt[n].offset_high = (handler >> 16) & 0xFFFF;
}

void rtc_isr() {
    time_rtc_handler();
    ports_outb(0x20, 0x20);
    ports_outb(0xA0, 0x20);
}

void idt_init(void) {
    ports_outb(0x20, 0x11);
    ports_outb(0xA0, 0x11);
    ports_outb(0x21, 0x20);
    ports_outb(0xA1, 0x28);
    ports_outb(0x21, 0x04);
    ports_outb(0xA1, 0x02);
    ports_outb(0x21, 0x01);
    ports_outb(0xA1, 0x01);

    uint8_t mask = ports_inb(0xA1);
    ports_outb(0xA1, mask & ~0x01);

    set_idt_gate(40, (uint32_t)rtc_isr);

    idtp.limit = (sizeof(struct IDTEntry) * 256) - 1;
    idtp.base = (uint32_t)&idt;
    load_idt(&idtp);
}
