#ifndef INFO_H
#define INFO_H

#include "multiboot.h"

void info_get_cpu_name(char* buffer);
uint32_t info_get_ram_amount(multiboot_info_t* mbi);
int bitmap_is_bit_set(size_t page_index);
void bitmap_set_bit(size_t page_index);
void bitmap_clear_bit(size_t page_index);
void mem_calculate_usage();
int mem_get_used_bytes();
int mem_get_total_bytes();
void mem_init(multiboot_info_t* mbi);
void system_systeminfo();

#endif