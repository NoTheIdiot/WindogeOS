// include files
#include <stdint.h>
#include <stddef.h>
#include "ports.h"

// inb
uint8_t ports_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

// outb
void ports_outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}

uint16_t ports_inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void ports_outw(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}