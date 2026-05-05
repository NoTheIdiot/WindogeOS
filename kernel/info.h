#ifndef INFO_H
#define INFO_H

#include "multiboot.h"

void info_get_cpu_name(char* buffer);
uint32_t info_get_ram_amount(multiboot_info_t* mbi);

#endif