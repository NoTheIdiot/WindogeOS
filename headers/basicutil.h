#ifndef BASICUTIL_H
#define BASICUTIL_H

// include headers
#include <stdint.h>
#include <stddef.h>

// port io and assembly wrappers
#include "../helper/basicutil/io.h"
#include "../helper/basicutil/others.h"

// basic instruction wrappers
void halt();
void core_shutdown(void);
void core_reboot(void);

extern const char* doge_ascii[22];

// serial stuff
void serial_init();
int serial_transmit_empty();
void serial_putchar(char c);
void serial_print(const char* str);
void serial_println(const char* str);

// basic
void log(const char* str);
void duolog(const char* str);
void panic(char* reason, char *file, int line);

// info
char* cpuid(void);

#endif