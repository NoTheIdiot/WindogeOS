#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#define MAX_LINES 1024
#define MAX_CHARS 256

typedef struct {
    char name[16];
    char extension[4];
    char content[1024][256];
} file_t;

void system_dogeshell();
void system_dogeshell_execute(char* command);

void system_info_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void system_info_ram(char *out_buffer);
void system_info_cpu(char *out_vendor);

void system_file_init();
file_t* system_file_search(char* filename);
void system_file_readfile(char* filename);
void system_file_list_directory();
void system_file_write_file(char* filename, char* string);

#endif
