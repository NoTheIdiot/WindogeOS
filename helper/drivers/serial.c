// include files
#include <dogeio.h>
#include <drivers.h>

// serial stuff for debugging and shit
void serial_init() {
    lowlevel_ports_outb(0x3F8 + 1, 0x00);    // delete interrupts from existance
    lowlevel_ports_outb(0x3F8 + 3, 0x80);
    lowlevel_ports_outb(0x3F8 + 0, 0x03);    // set baud rate to 38400 (what that actually means btw)
    lowlevel_ports_outb(0x3F8 + 1, 0x00);
    lowlevel_ports_outb(0x3F8 + 3, 0x03);    // set data bits to 8, parity to none, stop bits to 1
    lowlevel_ports_outb(0x3F8 + 2, 0xC7);    
    lowlevel_ports_outb(0x3F8 + 4, 0x0B);    // enable interrupts
}

// long ahh names lol
int serial_transmit_empty() {
    return lowlevel_ports_inb(0x3F8 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (serial_transmit_empty() == 0);
    lowlevel_ports_outb(0x3F8, c);
}

void serial_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        serial_putchar(str[i]);
    }
}

void serial_println(const char* str) {
    serial_print(str);
    serial_print("\n");
}