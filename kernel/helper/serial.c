#include <stdint.h>
#include <system.h>
#include <dogeio.h>

#define COM1_PORT 0x3F8

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
}

int sis_transmit_empty(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void sputchar(char c) {
    while (sis_transmit_empty() == 0);
    outb(COM1_PORT, c);
}

void sprint(const char *str) {
    while (*str) {
        sputchar(*str++);
    }
}

void sprintln(const char *str) {
    sprint(str);
    sprint("\n");
}