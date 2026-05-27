#include <stddef.h>
#include <stdint.h>
#include "ports.h"

void serial_write_char(char c) {
    while ((ports_inb(0x3F8 + 5) & 0x20) == 0);
    ports_outb(0x3F8, (uint8_t)c);
}

void serial_write_string(char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        serial_write_char(str[i]);
    }
}

void serial_write_hex(uint32_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_write_string("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uint8_t digit = (value >> shift) & 0xF;
        serial_write_char(hex[digit]);
    }
}
