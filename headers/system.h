#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#define MAX_LINES 1024
#define MAX_CHARS 256
extern char output[MAX_LINES][MAX_CHARS];
extern uint32_t range;
extern uint32_t rangehigh;
#define PARTITION_OFFSET 2048

void system_dogeshell();
void system_dogeshell_execute(const char* command);

void system_info_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void system_info_ram(char *out_buffer);
void system_info_cpu(char *out_vendor);

int system_file_init(struct limine_module_response *response);
void system_file_write_hardware(uint32_t lba, const uint8_t *buffer);
void system_file_read(uint32_t lba, uint8_t *buffer);
void system_file_format_83(const char* name, const char* ext, char* out_formatted);
int system_file_create(const char* short_name, const char* ext, const char* text_content, uint32_t content_len);
int system_file_list_directory(void);

#endif
