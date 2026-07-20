#include <basicutil.h>

#define SERIAL_PORT 0x3F8

void serial_init() {
	ports_outb(SERIAL_PORT + 1, 0x00);
	ports_outb(SERIAL_PORT + 3, 0x80);
	ports_outb(SERIAL_PORT + 0, 0x03);
	ports_outb(SERIAL_PORT + 1, 0x00);
	ports_outb(SERIAL_PORT + 3, 0x03);
	ports_outb(SERIAL_PORT + 2, 0xC7);
	ports_outb(SERIAL_PORT + 4, 0x0B);
}

int serial_transmit_empty() {
	return ports_inb(SERIAL_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    if (c == '\n') {
        while (serial_transmit_empty() == 0);
        ports_outb(SERIAL_PORT, (uint8_t)'\r');
    }
    while (serial_transmit_empty() == 0);
    ports_outb(SERIAL_PORT, (uint8_t)c);
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
