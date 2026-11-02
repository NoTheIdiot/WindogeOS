#include <stdint.h>
#include <basicutil.h>
#include "disk.h"

#define ATA_DATA        0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DF   0x20
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_BSY  0x80
#define ATA_TIMEOUT     1000000U

static uint8_t g_current_drive;
static uint8_t g_drive_present[ATA_DRIVE_COUNT];
static uint64_t g_drive_sector_counts[ATA_DRIVE_COUNT];

static void ata_delay_400ns(void) {
    for (volatile int i = 0; i < 4; i++) {
        (void)ports_inb(ATA_STATUS);
    }
}

static int ata_wait(int require_data) {
    for (uint32_t timeout = 0; timeout < ATA_TIMEOUT; timeout++) {
        uint8_t status = ports_inb(ATA_STATUS);
        if (status == 0 || status == 0xFF) {
            return -1;
        }
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return -1;
        }
        if (!(status & ATA_STATUS_BSY) &&
            (!require_data || (status & ATA_STATUS_DRQ))) {
            return 0;
        }
    }
    return -1;
}

int ata_select_drive(uint8_t drive) {
    if (drive >= ATA_DRIVE_COUNT) {
        return -1;
    }
    g_current_drive = drive;
    ports_outb(ATA_DRIVE_HEAD, (uint8_t)(0x40U | ((uint32_t)drive << 4)));
    ata_delay_400ns();
    return 0;
}

uint8_t ata_current_drive(void) {
    return g_current_drive;
}

int ata_drive_present(uint8_t drive) {
    return drive < ATA_DRIVE_COUNT && g_drive_present[drive] != 0;
}

uint64_t ata_drive_sector_count(uint8_t drive) {
    return drive < ATA_DRIVE_COUNT ? g_drive_sector_counts[drive] : 0;
}

void ata_probe_devices(void) {
    g_drive_present[ATA_PRIMARY_MASTER] = 0;
    g_drive_present[ATA_PRIMARY_SLAVE] = 0;
    g_drive_sector_counts[ATA_PRIMARY_MASTER] = 0;
    g_drive_sector_counts[ATA_PRIMARY_SLAVE] = 0;

    for (uint8_t drive = 0; drive < ATA_DRIVE_COUNT; drive++) {
        ports_outb(ATA_DRIVE_HEAD, (uint8_t)(0xA0U | ((uint32_t)drive << 4)));
        ata_delay_400ns();
        ports_outb(ATA_SECTOR_COUNT, 0);
        ports_outb(ATA_LBA_LOW, 0);
        ports_outb(ATA_LBA_MID, 0);
        ports_outb(ATA_LBA_HIGH, 0);
        ports_outb(ATA_COMMAND, 0xEC);

        uint8_t status = ports_inb(ATA_STATUS);
        if (status == 0 || status == 0xFF || ata_wait(0) != 0 ||
            ports_inb(ATA_LBA_MID) != 0 ||
            ports_inb(ATA_LBA_HIGH) != 0 ||
            ata_wait(1) != 0) {
            continue;
        }

        uint64_t lba48_sector_count = 0;
        uint32_t lba28_sector_count = 0;
        int supports_lba48 = 0;
        for (int word = 0; word < 256; word++) {
            uint16_t identify_word = ports_inw(ATA_DATA);
            if (word >= 100 && word <= 103) {
                lba48_sector_count |=
                    (uint64_t)identify_word << ((uint32_t)(word - 100) * 16);
            } else if (word == 60) {
                lba28_sector_count = identify_word;
            } else if (word == 61) {
                lba28_sector_count |= (uint32_t)identify_word << 16;
            } else if (word == 83) {
                supports_lba48 =
                    (identify_word & 0xC000U) == 0x4000U &&
                    (identify_word & (1U << 10)) != 0;
            }
        }
        g_drive_sector_counts[drive] =
            supports_lba48 && lba48_sector_count != 0
                ? lba48_sector_count
                : lba28_sector_count;
        g_drive_present[drive] = 1;
    }

    g_current_drive = ATA_PRIMARY_MASTER;
    ports_outb(ATA_DRIVE_HEAD, 0x40);
    ata_delay_400ns();
}

int ata_read_sector(uint8_t drive, uint64_t lba, uint8_t *buffer) {
    if (!ata_drive_present(drive) || buffer == 0 || lba >= (1ULL << 48)) {
        return -1;
    }
    if (ata_select_drive(drive) != 0) {
        return -1;
    }

    ports_outb(ATA_SECTOR_COUNT, 0);
    ports_outb(ATA_LBA_LOW, (uint8_t)(lba >> 24));
    ports_outb(ATA_LBA_MID, (uint8_t)(lba >> 32));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 40));
    ports_outb(ATA_SECTOR_COUNT, 1);
    ports_outb(ATA_LBA_LOW, (uint8_t)lba);
    ports_outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND, 0x24);

    if (ata_wait(1) != 0) {
        return -1;
    }

    uint16_t *words = (uint16_t *)buffer;
    for (int word = 0; word < 256; word++) {
        words[word] = ports_inw(ATA_DATA);
    }
    return 0;
}

int ata_write_sector(uint8_t drive, uint64_t lba, const uint8_t *buffer) {
    if (!ata_drive_present(drive) || buffer == 0 || lba >= (1ULL << 48)) {
        return -1;
    }
    if (ata_select_drive(drive) != 0) {
        return -1;
    }

    ports_outb(ATA_SECTOR_COUNT, 0);
    ports_outb(ATA_LBA_LOW, (uint8_t)(lba >> 24));
    ports_outb(ATA_LBA_MID, (uint8_t)(lba >> 32));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 40));
    ports_outb(ATA_SECTOR_COUNT, 1);
    ports_outb(ATA_LBA_LOW, (uint8_t)lba);
    ports_outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND, 0x34);

    if (ata_wait(1) != 0) {
        return -1;
    }

    const uint16_t *words = (const uint16_t *)buffer;
    for (int word = 0; word < 256; word++) {
        ports_outw(ATA_DATA, words[word]);
    }
    if (ata_wait(0) != 0) {
        return -1;
    }

    ports_outb(ATA_COMMAND, 0xEA);
    return ata_wait(0);
}
