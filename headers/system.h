#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

void system_info_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void system_info_ram(char *out_buffer);
void system_info_cpu(char *out_vendor);

void system_dogeshell();

#endif