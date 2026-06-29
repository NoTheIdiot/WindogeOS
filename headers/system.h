#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

void system_info_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void system_info_ram(char *out_buffer);
void system_info_cpu(char *out_vendor);

void system_dogeshell();

void system_file_read_sector(uint32_t lba, uint8_t *buffer);
void system_file_init();
uint32_t system_file_cluster_to_sector(uint32_t cluster);
uint32_t system_file_get_next_cluster(uint32_t current_cluster);
void system_file_list_directory();
void system_file_output_file(const char *filename);

#endif