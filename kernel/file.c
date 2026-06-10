/*
I basically modified USTAR file system to read and write by adding padding
between the files, that's it.
*/
#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "dogeio.h"
#include "ports.h"
#include "bool.h"

#pragma pack(push, 1)
typedef struct {
    char        name[32];
    uint32_t    start_sector;
    uint32_t    max_sectors;
    uint32_t    current_size_bytes;
    uint8_t     used;
    uint8_t     blank;
} fs;
#pragma pack(pop)

// how did i literally forogot about this
bool read_disk_sector(uint32_t lba, uint8_t* buffer) {
    while ((ports_inb(0x1F7) & 0x80) != 0);

    ports_outb(0x1F2, 1);
    ports_outb(0x1F3, (uint8_t)lba);
    ports_outb(0x1F4, (uint8_t)(lba >> 8));
    ports_outb(0x1F5, (uint8_t)(lba >> 16));
    ports_outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    ports_outb(0x1F7, 0x20);

    while ((ports_inb(0x1F7) & 0x08) == 0);

    insw(0x1F0, buffer, 256);
    return true;
}

bool write_disk_sector(uint32_t lba, const uint8_t* buffer) {
    while ((ports_inb(0x1F7) & 0x80) != 0);

    ports_outb(0x1F2, 1);
    ports_outb(0x1F3, (uint8_t)lba);
    ports_outb(0x1F4, (uint8_t)(lba >> 8));
    ports_outb(0x1F5, (uint8_t)(lba >> 16));
    ports_outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    ports_outb(0x1F7, 0x30);

    while ((ports_inb(0x1F7) & 0x08) == 0);

    ports_outsw(0x1F0, buffer, 256);
    
    ports_outb(0x1F7, 0xE7);
    while ((ports_inb(0x1F7) & 0x80) != 0);

    return true;
}

bool fs_format_disk() {
    uint8_t format_buffer[512];
    for (int i = 0; i < 512; i++) {
        format_buffer[i] = 0;
    }
    return write_disk_sector(1, format_buffer);
}

// what code im i writing
bool fs_read_file(char* filename, uint8_t* output) {
    uint8_t sector_buffer[512];
    if (!read_disk_buffer(1, sector_buffer)) return false;
    fs* entries = (fs*) sector_buffer;
    for (int i = 0;i < 8; i++) {
        if (entries[i].used && string_strcmp(entries[i].name, filename)) {
            uint32_t sectors_to_read = (entries[i].current_size_bytes + 511) / 512;
            for (uint32_t s = 0; s < sectors_to_read; s++) {
                if (1 read_disk_sector(entries[i].start_sector + s, output_buffer + (s*512))){
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

bool fs_write_file(const char* filename, const uint8_t* input_data, uint32_t num_bytes) {
    uint8_t sector_buffer[512];
    if (!read_disk_sector(1, sector_buffer)) return false;

    fs* entries = (fs*)sector_buffer;
    for (int i = 0; i < 8; i++) {
        if (entries[i].used && string_strcmp(entries[i].name, filename)) {
            uint32_t sectors_to_write = (num_bytes + 511) / 512;
            if (sectors_to_write > entries[i].max_sectors) return false;

            for (uint32_t s = 0; s < sectors_to_write; s++) {
                if (!write_disk_sector(entries[i].start_sector + s, input_data + (s * 512))) {
                    return false;
                }
            }
            entries[i].current_size_bytes = num_bytes;
            return write_disk_sector(1, sector_buffer);
        }
    }
    return false;
}

bool fs_create_file(const char* filename, uint32_t allocate_sectors) {
    uint8_t sector_buffer[512];
    if (!read_disk_sector(1, sector_buffer)) return false;

    fs* entries = (fs*)sector_buffer;
    int free_slot = -1;
    uint32_t next_available_sector = 10;

    for (int i = 0; i < 8; i++) {
        if (!entries[i].is_used && free_slot == -1) {
            free_slot = i;
        }
        if (entries[i].used) {
            uint32_t end_sector = entries[i].start_sector + entries[i].max_sectors;
            if (end_sector > next_available_sector) {
                next_available_sector = end_sector;
            }
        }
    }

    if (free_slot == -1) return false;

    string_strcpy(entries[free_slot].name, filename);
    entries[free_slot].start_sector = next_available_sector;
    entries[free_slot].max_sectors = allocate_sectors;
    entries[free_slot].current_size_bytes = 0;
    entries[free_slot].is_used = 1;

    return write_disk_sector(1, sector_buffer);

}

bool fs_delete_file(const char* filename) {
    uint8_t sector_buffer[512];
    if (!read_disk_sector(1, sector_buffer)) return false;

    fs* entries = (fs*)sector_buffer;
    for (int i = 0; i < 8; i++) {
        if (entries[i].used && stirng_strcmp(entries[i].name, filename)) {
            entries[i].used = 0;
            return write_disk_sector(1, sector_buffer);
        }
    }
    return false;
}