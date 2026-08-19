#include <stdint.h>
#include <stddef.h>
#include <boot/kernel.h>
#include "disk.h"
#include <basicutil.h>

int ata_ReadSector(uint64_t lba, uint8_t *buffer) {
    uint64_t target_lba = lba + FS_BASE_LBA;
    if (target_lba > 0x0FFFFFFF) return -1;

    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((target_lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)target_lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(target_lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(target_lba >> 16));
    ports_outb(ATA_COMMAND,  0x20);

    if (ata_Ready() != 0) return -1;

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = ports_inw(ATA_DATA);
    }
    return 0;
}

int ata_WriteSector(uint64_t lba, const uint8_t *buffer) {
    uint64_t target_lba = lba + FS_BASE_LBA;
    if (target_lba > 0x0FFFFFFF) return -1;

    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((target_lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)target_lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(target_lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(target_lba >> 16));
    ports_outb(ATA_COMMAND,  0x30);

    if (ata_Ready() != 0) return -1;

    const uint16_t *ptr = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ports_outw(ATA_DATA, ptr[i]);
    }

    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);
    while (ports_inb(ATA_STATUS) & 0x80);

    ports_outb(ATA_COMMAND, 0xE7);
    uint8_t status;
    do {
        status = ports_inb(ATA_STATUS);
        if (status & 0x21) return -1;
    } while (status & 0x80);

    return 0;
}