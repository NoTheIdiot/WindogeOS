#ifndef BASICUTIL_H
#define BASICUTIL_H

// include headers
#include <stdint.h>
#include <stddef.h>

// really basic stuff
void halt();

// low level ports
uint8_t ports_inb(uint16_t port);
void ports_outb(uint16_t port, uint8_t val);
uint16_t ports_inw(uint16_t port);
void ports_outw(uint16_t port, uint16_t data);
void ports_outsw(unsigned short port, const void *addr, unsigned long count);
void ports_insw(unsigned short port, void *addr, unsigned long count);

// serial stuff
void serial_init();
int serial_transmit_empty();
void serial_putchar(char c);
void serial_print(const char* str);
void serial_println(const char* str);

// basic
void log(const char* string);
void duolog(const char* string);

// info
char* cpuid(void);

#endif