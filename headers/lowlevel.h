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

#endif 