/*
random notes:
 - to anyone who wants to modify and call it your own, pls do delete this to avoid
   your drama

This is just the development of an actualy file system, do not delete fs.c.
This is a part of dogeio header file.
*/

#include <dogeio.h>
#include <stdint.h>
#include <stddef.h>
#include <basicutil.h>

#define ATA_DATA        0x1F0
#define ATA_FEATURES    0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7

// testing, remove after everything works
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

// function to wait till ata is ready.
// just waits till the port responses with... *something*
static void ata_Ready(void) {
    while ((ports_inb(ATA_STATUS) & 0x80) || !(ports_inb(ATA_STATUS) & 0x40));
}

static void ata_ReadSector(uint32_t lba, uint8_t *buffer) {
    ata_Ready();

    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND,  0x20);

    ata_Ready();
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = ports_inw(ATA_DATA);
    }
}

static void ata_WriteSector(uint16_t lba, const uint8_t *buffer) {
    ata_Ready();

    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND,  0x30); // ATA Write Sector

    ata_Ready();

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ports_outw(ATA_DATA, ptr[i]);
    }
    
    ports_outb(ATA_COMMAND, 0xE7); 
    ata_Ready();
}
#pragma clang diagnostic pop