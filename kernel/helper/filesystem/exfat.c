#include <dogeio.h>
#include <system.h>
#include <stdint.h>
#include <string.h>

#define PARTITION_OFFSET 4096

// isn't it obvious what this does?
static inline uint64_t cluster_to_sectors(const mount_t *vol, uint32_t cluster) {
    return vol->heap_offset_bytes + ((uint64_t)(cluster - 2) * vol->sectors_per_cluster);
}

static inline int read_offset(mount_t* vol, uint64_t rel_sector, void* buf, size_t sector_count) {
    uint8_t *byte_buf = (uint8_t *)buf;
    uint8_t drive = ata_current_drive();

    for (size_t i = 0; i < sector_count; i++) {
        uint64_t lba = vol->poffset_bytes + PARTITION_OFFSET + rel_sector + i;
        if (ata_read_sector(drive, lba, byte_buf + (i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}


uint32_t next_cluster(mount_t *vol, uint32_t current_cluster, uint8_t *sector_buf) {
    uint64_t fat_byte_offset = (uint64_t)current_cluster * 4;
    uint64_t fat_sector      = vol->offset_bytes + (fat_byte_offset / vol->bytes_per_sector);
    uint32_t entry_offset    = fat_byte_offset % vol->bytes_per_sector;

    if (read_offset(vol, fat_sector, sector_buf, 1) != 0) {
        return 0xFFFFFFFF;
    }
    return *(uint32_t *)&sector_buf[entry_offset];
}

// mount a damn drive.
int mount(mount_t *vol, uint8_t *cluster_buf) {
    exfat_start_sector boot;
    vol->poffset_bytes = 0;

    if (read_offset(vol, 0, &boot, 1) != 0) {
        sprintln("[BAD] disk read failure, SATA driver bad.");
        return -256;
    }

    sprintln("[HDD] verifying boot magic and exfat magic");
    if (boot.sig != 0xAA55) {
        sprintln("[HDD] uh, invalid VBR. (0xAA55)");
        return -128;
    }

    sprintln("[HDD] VBR valid");
    sprintln("[HDD] checking exFAT magic");
    const char* magic = "EXFAT   ";
    for (int i = 0; i < 8; i++) {
        if (boot.fs_name[i] != magic[i]) {
            sprintln("[HDD] uh sir, this is not exFAT");
            return -128;
        }
    }

    vol->bytes_per_sector    = 1U << boot.sector_shift;
    vol->sectors_per_cluster = 1U << boot.sectors_pch;
    vol->bytes_per_cluster   = vol->bytes_per_sector * vol->sectors_per_cluster;

    uint32_t active_fat    = (boot.volume_flags & 0x01) ? 1 : 0;
    vol->offset_bytes      = (uint64_t)boot.foffset + ((uint64_t)active_fat * boot.flength);
    vol->heap_offset_bytes = (uint64_t)boot.choffset;
    vol->root_cluster      = boot.fcord;
    vol->cluster_count     = boot.ccount;

    vol->bitmap_cluster = 0;
    vol->upcase_cluster = 0;

    uint32_t curr = vol->root_cluster;
    while (curr >= 2 && curr < 0xFFFFFFF8) {
        uint64_t dir_sector = cluster_to_sectors(vol, curr);
        if (read_offset(vol, dir_sector, cluster_buf, vol->sectors_per_cluster) != 0) {
            sprintln("[BAD] disk read failure, SATA driver bad.");
            return -256;
        }

        for (uint32_t offset = 0; offset < vol->bytes_per_cluster; offset += 32) {
            uint8_t type = cluster_buf[offset];

            if (type == 0x00) {
                goto done;
            }

            if (type == 0x81) {
                bitmap *bm = (bitmap *)&cluster_buf[offset];
                if ((bm->flags & 0x01) == active_fat) {
                    vol->bitmap_cluster = bm->first_cluster;
                    vol->bitmap_size    = bm->data_length;
                }
            } else if (type == 0x82) {
                upcase *uc = (upcase *)&cluster_buf[offset];
                vol->upcase_cluster = uc->first_cluster;
                vol->upcase_size    = uc->size;
            }
        }

        curr = next_cluster(vol, curr, cluster_buf);
    }

done:
    if (vol->bitmap_cluster < 2 || vol->upcase_cluster < 2) {
        sprintln("[HDD] missing bitmap or upcase table");
        return -128;
    }

    sprintln("[HDD] exFAT mounted");
    return 0;
}
