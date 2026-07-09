#ifndef LOWLEVEL_H
#define LOWLEVEL_H

#include <stdint.h>

// ports
uint8_t lowlevel_ports_inb(uint16_t port);
void lowlevel_ports_outb(uint16_t port, uint8_t val);
uint16_t lowlevel_ports_inw(uint16_t port);
void lowlevel_ports_outw(uint16_t port, uint16_t data);
void lowlevel_ports_outsw(unsigned short port, const void *addr, unsigned long count);
void lowlevel_ports_insw(unsigned short port, void *addr, unsigned long count);

void serial_init();
int serial_transmit_empty();
void serial_putchar(char c);
void serial_print(const char* str);
void serial_println(const char* str);

// idt stuff
typedef struct {
    uint16_t offset_1;        // Offset bits 0..15
    uint16_t selector;        // GDT Kernel Code Segment Selector (usually 0x08)
    uint8_t  ist;             // Interrupt Stack Table (set to 0)
    uint8_t  type_attr;       // Type and attributes (0x8E for Interrupt Gates)
    uint16_t offset_2;        // Offset bits 16..31
    uint32_t offset_3;        // Offset bits 32..63
    uint32_t reserved;        // Reserved, must be 0
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

void idt_init(void);

#endif 