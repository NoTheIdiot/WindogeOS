// include files
#include <dogeio.h>
#include <lowlevel.h>

// serial stuff for debugging and shit
void dogeio_text_serial_init() {
    lowlevel_ports_outb(0x3F8 + 1, 0x00);    // delete interrupts from existance
    lowlevel_ports_outb(0x3F8 + 3, 0x80);
    lowlevel_ports_outb(0x3F8 + 0, 0x03);    // set baud rate to 38400 (what that actually means btw)
    lowlevel_ports_outb(0x3F8 + 1, 0x00);
    lowlevel_ports_outb(0x3F8 + 3, 0x03);    // set data bits to 8, parity to none, stop bits to 1
    lowlevel_ports_outb(0x3F8 + 2, 0xC7);    
    lowlevel_ports_outb(0x3F8 + 4, 0x0B);    // enable interrupts
}

// long ahh names lol
int dogeio_text_serial_transmit_empty() {
    return lowlevel_ports_inb(0x3F8 + 5) & 0x20;
}

void dogeio_text_serial_putchar(char c) {
    while (dogeio_text_serial_transmit_empty() == 0);
    lowlevel_ports_outb(0x3F8, c);
}

void dogeio_text_serial_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        dogeio_text_serial_putchar(str[i]);
    }
}

void dogeio_text_serial_println(const char* str) {
    dogeio_text_serial_print(str);
    dogeio_text_serial_print("\n");
}