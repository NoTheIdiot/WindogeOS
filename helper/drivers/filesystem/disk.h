#ifndef DISK_H
#define DISK_H

#include <stdint.h>

#define ATA_PRIMARY_MASTER 0
#define ATA_PRIMARY_SLAVE  1
#define ATA_DRIVE_COUNT    2

void ata_probe_devices(void);
int ata_drive_present(uint8_t drive);
uint64_t ata_drive_sector_count(uint8_t drive);
int ata_select_drive(uint8_t drive);
uint8_t ata_current_drive(void);
int ata_read_sector(uint8_t drive, uint64_t lba, uint8_t *buffer);
int ata_write_sector(uint8_t drive, uint64_t lba, const uint8_t *buffer);

#endif
