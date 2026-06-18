#pragma once

#include <stdint.h>

void disk_init(char* path);

void writeSectors(const void* buffer, uint32_t lba, uint32_t sector_num);
void readSectors(void* buffer, uint32_t lba, uint32_t sector_num);