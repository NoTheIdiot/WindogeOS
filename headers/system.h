#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

void system_dogeshell();
void system_dogeshell_execute(const char* command);

void system_info_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void system_info_ram(char *out_buffer);
void system_info_cpu(char *out_vendor);

int system_file_init(struct limine_module_response *response);
int system_file_list_directory(void);
int system_file_readfile(const char *header_check);

#endif
