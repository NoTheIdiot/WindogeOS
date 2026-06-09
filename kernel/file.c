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
}
#pragma pack(pop)

bool read_disk_sector(uint32_t lba, uint8_t* buffer) {
    while ((inb(0x1F7) & 0x80) != 0);

    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F7, 0x20);

    while ((inb(0x1F7) & 0x08) == 0);

    insw(0x1F0, buffer, 256);
    return true;
}

bool write_disk_sector(uint32_t lba, const uint8_t* buffer) {
    while ((inb(0x1F7) & 0x80) != 0);

    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F7, 0x30);

    while ((inb(0x1F7) & 0x08) == 0);

    outsw(0x1F0, buffer, 256);
    
    outb(0x1F7, 0xE7);
    while ((inb(0x1F7) & 0x80) != 0);

    return true;
}

bool fs_format_disk() {
    uint8_t format_buffer[512];
    for (int i = 0; i < 512; i++) {
        format_buffer[i] = 0;
    }
    return write_disk_sector(1, format_buffer);
}