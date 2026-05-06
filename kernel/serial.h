#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void serial_write_char(char c);
void serial_write_string(char *string);
void serial_write_hex(uint32_t value);

#endif