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

static inline int write_offset(mount_t* vol, uint64_t rel_sector, void* buf, size_t sector_count) {
    uint8_t *byte_buf = (uint8_t *)buf;
    uint8_t drive = ata_current_drive();

    for (size_t i = 0; i < sector_count; i++) {
        uint64_t lba = vol->poffset_bytes + PARTITION_OFFSET + rel_sector + i;
        if (ata_write_sector(drive, lba, byte_buf + (i * 512)) != 0) {
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

static uint16_t calc_name_hash(const char *name, size_t len) {
    uint16_t hash = 0;
    for (size_t i = 0; i < len; i++) {
        uint16_t ch = (uint8_t)name[i];
        if (ch >= 'a' && ch <= 'z') {
            ch -= 32; 
        }
        hash = ((hash & 1) ? 0x8000 : 0) + (hash >> 1) + (ch & 0xFF);
        hash = ((hash & 1) ? 0x8000 : 0) + (hash >> 1) + ((ch >> 8) & 0xFF);
    }
    return hash;
}

static uint16_t calc_set_checksum(const uint8_t *entries, size_t total_entries) {
    uint16_t checksum = 0;
    size_t total_bytes = total_entries * 32;
    for (size_t i = 0; i < total_bytes; i++) {
        if (i == 2 || i == 3) continue;
        checksum = ((checksum & 1) ? 0x8000 : 0) + (checksum >> 1) + entries[i];
    }
    return checksum;
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

int create_file(mount_t *vol, const char *name) {
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len > 255) return -1;

    uint8_t name_entries = (name_len + 14) / 15;
    uint8_t needed_slots = 2 + name_entries;

    uint8_t entry_set[19 * 32];
    memset(entry_set, 0, needed_slots * 32);

    // 0x85: file_entry
    file_entry *fe = (file_entry *)entry_set;
    fe->entry_type = 0x85;
    fe->scount     = 1 + name_entries;
    fe->attribute  = 0x0020; // Archive

    // stream entry 
    stream_entry *se    = (stream_entry *)(entry_set + 32);
    se->entry_type      = 0xC0;
    // allocation is possible
    se->secondary_flags = 0x01;
    se->name_length     = (uint8_t)name_len;
    se->name_hash       = calc_name_hash(name, name_len);

    // file name entry
    for (uint8_t i = 0; i < name_entries; i++) {
        file_name_entry *fn = (file_name_entry *)(entry_set + (2 + i) * 32);
        fn->entry_type = 0xC1;

        uint16_t *utf16 = (uint16_t *)fn->filename;
        for (size_t j = 0; j < 15; j++) {
            size_t idx = (i * 15) + j;
            utf16[j] = (idx < name_len) ? (uint8_t)name[idx] : 0x0000;
        }
    }

    fe->checksum = calc_set_checksum(entry_set, needed_slots);

    // scan root dir
    uint8_t sector_buf[512];
    uint64_t base_sector = cluster_to_sectors(vol, vol->root_cluster);
    uint32_t total_sectors = vol->bytes_per_cluster / 512;

    uint32_t free_run = 0;
    uint32_t start_slot = 0;

    for (uint32_t sec = 0; sec < total_sectors; sec++) {
        if (read_offset(vol, base_sector + sec, sector_buf, 1) != 0) return -1;

        for (uint32_t slot = 0; slot < 16; slot++) {
            uint32_t global_slot = (sec * 16) + slot;
            uint8_t type = sector_buf[slot * 32];

            if (type == 0x00 || (type & 0x80) == 0) {
                if (free_run == 0) start_slot = global_slot;
                free_run++;
                if (free_run == needed_slots) goto found;
            } else {
                free_run = 0;
            }
        }
    }
    return -2;

found:
    for (uint8_t i = 0; i < needed_slots; i++) {
        uint32_t target_slot = start_slot + i;
        uint32_t sec  = target_slot / 16;
        uint32_t slot = target_slot % 16;

        if (read_offset(vol, base_sector + sec, sector_buf, 1) != 0) return -1;
        memcpy(&sector_buf[slot * 32], &entry_set[i * 32], 32);
        if (write_offset(vol, base_sector + sec, sector_buf, 1) != 0) return -1;
    }

    return 0;
}