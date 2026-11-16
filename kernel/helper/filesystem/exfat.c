#include <dogeio.h>
#include <system.h>
#include <stdint.h>
#include <string.h>

#define PARTITION_OFFSET 4096
#define SECTOR_SIZE 512
#define WINDOW_SIZE 1024

// isn't it obvious what this does?
static const char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    if (c == '\0') {
        return s;
    }
    return last;
}

static char g_cwd[256] = "/";

static void build_path(const char *path, char *out)
{
    if (!path || !out)
        return;

    if (path[0] == '/') {
        strcpy(out, path);
    } else {
        if (strcmp(g_cwd, "/") == 0) {
            strcpy(out, "/");
            strcat(out, path);
        } else {
            strcpy(out, g_cwd);
            strcat(out, "/");
            strcat(out, path);
        }
    }

    char normalized[256];
    char *parts[32];
    int count = 0;

    strcpy(normalized, out);

    char *p = normalized;

    while (*p) {

        while (*p == '/')
            p++;

        if (!*p)
            break;

        char *start = p;

        while (*p && *p != '/')
            p++;

        if (*p) {
            *p = '\0';
            p++;
        }

        if (strcmp(start, ".") == 0) {
            continue;
        }

        if (strcmp(start, "..") == 0) {
            if (count > 0)
                count--;
            continue;
        }

        parts[count++] = start;
    }

    strcpy(out, "/");

    for (int i = 0; i < count; i++) {

        strcat(out, parts[i]);

        if (i != count - 1)
            strcat(out, "/");
    }

    if (count == 0)
        strcpy(out, "/");
}

static inline uint64_t cluster_to_sectors(const mount_t *vol, uint32_t cluster) {
    return vol->heap_offset_bytes + ((uint64_t)(cluster - 2) * vol->sectors_per_cluster);
}

static inline int read_offset(mount_t* vol, uint64_t rel_sector, uint8_t* buf, size_t sector_count) {
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

static inline int write_offset(mount_t* vol, uint64_t rel_sector, uint8_t* buf, size_t sector_count) {
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

static uint32_t alloc_one_cluster(mount_t *vol) {
    uint8_t sec_buf[512];
    uint64_t bm_start_sector  = cluster_to_sectors(vol, vol->bitmap_cluster);
    uint32_t total_bm_sectors = (vol->bitmap_size + 511) / 512;

    for (uint32_t sec = 0; sec < total_bm_sectors; sec++) {
        if (read_offset(vol, bm_start_sector + sec, sec_buf, 1) != 0) {
            return 0;
        }

        for (uint32_t i = 0; i < 512; i++) {
            if (sec_buf[i] == 0xFF) continue;

            for (uint8_t b = 0; b < 8; b++) {
                if ((sec_buf[i] & (1 << b)) == 0) {
                    uint32_t bit_index = (sec * 4096) + (i * 8) + b;
                    uint32_t cluster   = bit_index + 2;

                    if (cluster >= vol->cluster_count + 2) return 0;

                    sec_buf[i] |= (1 << b);
                    if (write_offset(vol, bm_start_sector + sec, sec_buf, 1) != 0) {
                        return 0;
                    }

                    return cluster;
                }
            }
        }
    }
    return 0;
}

static int set_fat_entry(mount_t *vol, uint32_t cluster, uint32_t value) {
    uint8_t sec_buf[512];
    uint64_t fat_byte_offset = (uint64_t)cluster * 4;
    uint64_t fat_sector      = vol->offset_bytes + (fat_byte_offset / 512);
    uint32_t entry_offset    = fat_byte_offset % 512;

    if (read_offset(vol, fat_sector, sec_buf, 1) != 0) return -1;
    *(uint32_t *)&sec_buf[entry_offset] = value;
    return write_offset(vol, fat_sector, sec_buf, 1);
}

static int set_bitmap_bit(mount_t *vol, uint32_t cluster, uint8_t used) {
    if (cluster < 2 || cluster >= vol->cluster_count + 2) return -1;

    uint8_t sec_buf[512];
    uint32_t bit_index = cluster - 2;
    uint32_t sec_idx   = bit_index / 4096;
    uint32_t byte_idx  = (bit_index % 4096) / 8;
    uint8_t  bit_mask  = 1 << (bit_index % 8);

    uint64_t bm_sector = cluster_to_sectors(vol, vol->bitmap_cluster) + sec_idx;
    if (read_offset(vol, bm_sector, sec_buf, 1) != 0) return -1;

    if (used) sec_buf[byte_idx] |= bit_mask;
    else        sec_buf[byte_idx] &= ~bit_mask;

    return write_offset(vol, bm_sector, sec_buf, 1);
}

static void free_cluster_chain(mount_t *vol, uint32_t first_cluster, uint64_t data_length, uint8_t no_fat_chain) {
    if (first_cluster < 2 || data_length == 0) return;

    if (no_fat_chain) {
        uint32_t count = (data_length + vol->bytes_per_cluster - 1) / vol->bytes_per_cluster;
        for (uint32_t i = 0; i < count; i++) {
            set_bitmap_bit(vol, first_cluster + i, 0);
        }
    } else {
        uint8_t fat_buf[512];
        uint32_t curr = first_cluster;
        while (curr >= 2 && curr < 0xFFFFFFF8) {
            uint32_t next = next_cluster(vol, curr, fat_buf);
            set_bitmap_bit(vol, curr, 0);
            set_fat_entry(vol, curr, 0x00000000);
            curr = next;
        }
    }
}

static uint32_t alloc_cluster_near(mount_t *vol, uint32_t hint) {
    uint8_t sec_buf[512];
    uint64_t bm_base = cluster_to_sectors(vol, vol->bitmap_cluster);
    uint32_t total_bm_secs = (vol->bitmap_size + 511) / 512;
    uint32_t start_sec = (hint >= 2) ? ((hint - 2) / 4096) : 0;

    for (uint32_t pass = 0; pass < total_bm_secs; pass++) {
        uint32_t sec = (start_sec + pass) % total_bm_secs;
        if (read_offset(vol, bm_base + sec, sec_buf, 1) != 0) return 0;

        for (uint32_t i = 0; i < 512; i++) {
            if (sec_buf[i] == 0xFF) continue;
            for (uint8_t b = 0; b < 8; b++) {
                if ((sec_buf[i] & (1 << b)) == 0) {
                    uint32_t cluster = (sec * 4096) + (i * 8) + b + 2;
                    if (cluster >= vol->cluster_count + 2) continue;

                    sec_buf[i] |= (1 << b);
                    if (write_offset(vol, bm_base + sec, sec_buf, 1) != 0) return 0;
                    return cluster;
                }
            }
        }
    }
    return 0;
}

static int find_free_dir_slots(
    mount_t *vol,
    uint32_t dir_cluster,
    uint32_t needed_slots,
    uint32_t *out_cluster,
    uint32_t *out_start_slot)
{
    uint8_t sector_buf[512];
    uint8_t fat_buf[512];

    uint32_t curr = dir_cluster;

    while (curr >= 2 && curr < 0xFFFFFFF8) {

        uint32_t free_run = 0;
        uint32_t start_slot = 0;

        uint64_t base_sector = cluster_to_sectors(vol, curr);
        uint32_t sectors = vol->bytes_per_cluster / 512;

        for (uint32_t sec = 0; sec < sectors; sec++) {

            if (read_offset(vol,
                            base_sector + sec,
                            sector_buf,
                            1) != 0)
                return -1;

            for (uint32_t slot = 0; slot < 16; slot++) {

                uint32_t global_slot = sec * 16 + slot;
                uint8_t type = sector_buf[slot * 32];

                if (type == 0x00 || !(type & 0x80)) {

                    if (!free_run)
                        start_slot = global_slot;

                    free_run++;

                    if (free_run >= needed_slots) {

                        *out_cluster = curr;
                        *out_start_slot = start_slot;
                        return 0;
                    }

                } else {
                    free_run = 0;
                }
            }
        }

        uint32_t next = next_cluster(vol, curr, fat_buf);

        if (next < 2 || next >= 0xFFFFFFF8)
            break;

        curr = next;
    }

    uint32_t new_cluster = alloc_cluster_near(vol, curr + 1);
    if (new_cluster < 2)
        return -2;

    set_fat_entry(vol, curr, new_cluster);
    set_fat_entry(vol, new_cluster, 0xFFFFFFFF);

    uint8_t zero[512];
    memset(zero, 0, sizeof(zero));

    uint64_t base = cluster_to_sectors(vol, new_cluster);

    for (uint32_t s = 0; s < vol->sectors_per_cluster; s++) {

        if (write_offset(vol,
                         base + s,
                         zero,
                         1) != 0)
            return -3;
    }

    *out_cluster = new_cluster;
    *out_start_slot = 0;

    return 0;
}

static int write_entry_set(mount_t *vol, uint32_t cluster, uint32_t start_slot, const uint8_t *entries, uint32_t total_slots)
{
    uint8_t secbuf[512];

    uint64_t base_sector =
        cluster_to_sectors(vol, cluster);

    for (uint32_t i = 0; i < total_slots; i++) {

        uint32_t target = start_slot + i;

        uint32_t sec =
            target / 16;

        uint32_t slot =
            target % 16;

        if (read_offset(vol, base_sector + sec, secbuf, 1) != 0) return -1;

        memcpy(&secbuf[slot * 32], &entries[i * 32], 32);

        if (write_offset(vol, base_sector + sec, secbuf, 1) != 0)
            return -1;
    }

    return 0;
}

static int extend_directory(mount_t *vol, uint32_t dir_cluster, uint32_t *new_cluster_out) {
    uint8_t fat_buf[512];

    uint32_t last = dir_cluster;

    while (1) {
        uint32_t next = next_cluster(vol, last, fat_buf);

        if (next < 2 || next >= 0xFFFFFFF8)
            break;

        last = next;
    }

    uint32_t new_cluster = alloc_cluster_near(vol, last + 1);
    if (new_cluster < 2)
        return -1;

    set_fat_entry(vol, new_cluster, 0xFFFFFFFF);
    set_fat_entry(vol, last, new_cluster);

    uint8_t zero[512];
    memset(zero, 0, sizeof(zero));

    uint64_t base = cluster_to_sectors(vol, new_cluster);

    for (uint32_t s = 0; s < vol->sectors_per_cluster; s++) {
        if (write_offset(vol, base + s, zero, 1) != 0)
            return -1;
    }

    *new_cluster_out = new_cluster;
    return 0;
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

    uint32_t active_fat      = (boot.volume_flags & 0x01) ? 1 : 0;
    vol->offset_bytes        = (uint64_t)boot.foffset + ((uint64_t)active_fat * boot.flength);
    vol->heap_offset_bytes = (uint64_t)boot.choffset;
    vol->root_cluster        = boot.fcord;
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

static void split_path(const char *path, char *parent, char *name) {
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        strcpy(parent, "/");
        strcpy(name, path);
    } else if (last_slash == path) {
        strcpy(parent, "/");
        strcpy(name, last_slash + 1);
    } else {
        size_t len = last_slash - path;
        memcpy(parent, path, len);
        parent[len] = '\0';
        strcpy(name, last_slash + 1);
    }
}

int find_entry_in_dir(mount_t *vol, uint32_t dir_cluster, const char *name, file_entry *out_fe, stream_entry *out_se, uint32_t *out_slot_index, uint32_t *out_cluster) {
    size_t name_len = strlen(name);
    uint8_t window[WINDOW_SIZE];
    uint8_t fat_buf[SECTOR_SIZE];
    uint32_t curr = dir_cluster;

    while (curr >= 2 && curr < 0xFFFFFFF8) {
        uint64_t dir_sector = cluster_to_sectors(vol, curr);
        uint32_t dir_sectors = vol->bytes_per_cluster / SECTOR_SIZE;

        for (uint32_t sec = 0; sec < dir_sectors; sec++) {
            if (read_offset(vol, dir_sector + sec, window, 1) != 0) return -1;
            if (sec + 1 < dir_sectors) {
                read_offset(vol, dir_sector + sec + 1, window + SECTOR_SIZE, 1);
            } else {
                memset(window + SECTOR_SIZE, 0, SECTOR_SIZE);
            }

            for (uint32_t slot = 0; slot < 16; slot++) {
                uint8_t *ptr = &window[slot * 32];
                if (*ptr == 0x00) return -2;
                if (*ptr != 0x85) continue;

                file_entry *fe = (file_entry *)ptr;
                stream_entry *se = (stream_entry *)(ptr + 32);
                if (se->entry_type != 0xC0 || se->name_length != name_len) continue;

                uint8_t match = 1;
                for (size_t i = 0; i < name_len; i++) {
                    file_name_entry *fn = (file_name_entry *)(ptr + (2 + (i / 15)) * 32);
                    uint16_t *utf16 = (uint16_t *)fn->filename;
                    if (utf16[i % 15] != (uint8_t)name[i]) {
                        match = 0;
                        break;
                    }
                }

                if (match) {
                    if (out_fe) memcpy(out_fe, fe, sizeof(file_entry));
                    if (out_se) memcpy(out_se, se, sizeof(stream_entry));
                    if (out_slot_index) *out_slot_index = (sec * 16) + slot;
                    if (out_cluster) *out_cluster = curr;
                    return 0;
                }
            }
        }
        curr = next_cluster(vol, curr, fat_buf);
    }
    return -2;
}

uint32_t resolve_path(mount_t *vol, const char *path) {
    if (!vol || !path || path[0] == '\0') return 0;

    uint32_t current_cluster = vol->root_cluster;
    const char *p = path;

    while (*p == '/') p++;
    if (*p == '\0') return current_cluster;

    char component[256];
    while (*p != '\0') {
        size_t len = 0;
        while (p[len] != '\0' && p[len] != '/') {
            if (len >= sizeof(component) - 1) return 0;
            component[len] = p[len];
            len++;
        }
        component[len] = '\0';

        file_entry fe;
        stream_entry se;
        if (find_entry_in_dir(vol, current_cluster, component, &fe, &se, NULL, NULL) != 0) {
            return 0;
        }
        if (!(fe.attribute & 0x0010)) {
            return 0;
        }

        current_cluster = se.first_cluster;
        p += len;
        while (*p == '/') p++;
    }

    return current_cluster;
}

int create_dir(mount_t *vol, const char *path)
{
    char full[256];
    build_path(path, full);

    char parent_path[256];
    char dir_name[256];

    split_path(full, parent_path, dir_name);

    uint32_t parent_cluster = resolve_path(vol, parent_path);
    if (parent_cluster == 0)
        return -2;

    if (find_entry_in_dir(vol,
                          parent_cluster,
                          dir_name,
                          NULL,
                          NULL,
                          NULL,
                          NULL) == 0)
    {
        return -4; // already exists
    }

    size_t name_len = strlen(dir_name);

    if (name_len == 0 || name_len > 255)
        return -1;

    uint8_t name_entries = (name_len + 14) / 15;
    uint8_t needed_slots = 2 + name_entries;

    uint8_t entry_set[19 * 32];
    memset(entry_set, 0, sizeof(entry_set));

    uint32_t dir_cluster =
        alloc_cluster_near(vol, parent_cluster);

    if (dir_cluster < 2)
        return -3;

    set_fat_entry(vol, dir_cluster, 0xFFFFFFFF);

    uint8_t zero[512];
    memset(zero, 0, sizeof(zero));

    uint64_t dir_sector =
        cluster_to_sectors(vol, dir_cluster);

    for (uint32_t s = 0; s < vol->sectors_per_cluster; s++) {
        if (write_offset(vol, dir_sector + s, zero, 1) != 0)
            return -4;
    }

    file_entry *fe = (file_entry *)entry_set;

    fe->entry_type = 0x85;
    fe->scount     = 1 + name_entries;
    fe->attribute  = 0x0010;

    stream_entry *se = (stream_entry *)(entry_set + 32);

    se->entry_type         = 0xC0;
    se->secondary_flags    = 0x03;
    se->name_length        = (uint8_t)name_len;
    se->name_hash          = calc_name_hash(dir_name, name_len);
    se->first_cluster      = dir_cluster;
    se->valid_data_length  = vol->bytes_per_cluster;
    se->data_length        = vol->bytes_per_cluster;

    for (uint8_t i = 0; i < name_entries; i++) {

        file_name_entry *fn =
            (file_name_entry *)(entry_set + ((uint32_t)i + 2) * 32);

        fn->entry_type = 0xC1;

        uint16_t *utf16 =
            (uint16_t *)fn->filename;

        for (uint32_t j = 0; j < 15; j++) {

            uint32_t idx =
                (i * 15) + j;

            utf16[j] =
                (idx < name_len)
                ? (uint8_t)dir_name[idx]
                : 0;
        }
    }

    fe->checksum = calc_set_checksum(entry_set, needed_slots);

    uint32_t target_cluster;
    uint32_t target_slot;

    int rc =
        find_free_dir_slots(vol, parent_cluster, needed_slots, &target_cluster, &target_slot);

    if (rc != 0)
        return rc;

    return write_entry_set(vol, target_cluster, target_slot, entry_set, needed_slots);
}

int create_file(mount_t *vol, const char *path)
{
    char full[256];
    build_path(path, full);

    char parent_path[256];
    char file_name[256];

    split_path(full, parent_path, file_name);

    uint32_t parent_cluster = resolve_path(vol, parent_path);
    if (parent_cluster == 0)
        return -2;

    if (find_entry_in_dir(vol,
                          parent_cluster,
                          file_name,
                          NULL,
                          NULL,
                          NULL,
                          NULL) == 0)
    {
        return -4; // already exists
    }

    size_t name_len = strlen(file_name);

    if (name_len == 0 || name_len > 255)
        return -1;

    uint8_t name_entries = (name_len + 14) / 15;
    uint8_t needed_slots = 2 + name_entries;

    uint8_t entry_set[19 * 32];
    memset(entry_set, 0, sizeof(entry_set));

    file_entry *fe = (file_entry *)entry_set;

    fe->entry_type = 0x85;
    fe->scount     = 1 + name_entries;
    fe->attribute  = 0x0020;

    stream_entry *se = (stream_entry *)(entry_set + 32);

    se->entry_type      = 0xC0;
    se->secondary_flags = 0x01;
    se->name_length     = (uint8_t)name_len;
    se->name_hash       = calc_name_hash(file_name, name_len);

    for (uint8_t i = 0; i < name_entries; i++) {

        file_name_entry *fn =
            (file_name_entry *)(entry_set + ((uint32_t)i + 2) * 32);

        fn->entry_type = 0xC1;

        uint16_t *utf16 = (uint16_t *)fn->filename;

        for (uint32_t j = 0; j < 15; j++) {

            uint32_t idx = (i * 15) + j;

            utf16[j] =
                (idx < name_len)
                    ? (uint8_t)file_name[idx]
                    : 0;
        }
    }

    fe->checksum =
        calc_set_checksum(entry_set, needed_slots);

    uint32_t target_cluster;
    uint32_t target_slot;

    int rc = find_free_dir_slots(vol, parent_cluster, needed_slots, &target_cluster,  &target_slot);

    if (rc != 0)
        return rc;

    return write_entry_set(vol, target_cluster, target_slot, entry_set, needed_slots);
}

int write_file(mount_t *vol, const char *path, const uint8_t *data, uint64_t size)
{
    char full[256];
    build_path(path, full);

    char parent_path[256];
    char file_name[256];

    split_path(full, parent_path, file_name);


    uint32_t parent_cluster = resolve_path(vol, parent_path);
    if (parent_cluster == 0) return -2;

    file_entry fe;
    stream_entry se;
    uint32_t slot_idx, cluster;
    if (find_entry_in_dir(vol, parent_cluster, file_name, &fe, &se, &slot_idx, &cluster) != 0) {
        return -2;
    }

    free_cluster_chain(vol, se.first_cluster, se.data_length, se.secondary_flags & 0x02);

    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;
    uint8_t no_fat_chain = 0;
    uint64_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t curr_cluster = alloc_cluster_near(vol, prev_cluster + 1);
        if (curr_cluster < 2) return -3;

        if (first_cluster == 0) {
            first_cluster = curr_cluster;
            no_fat_chain = 1;
        } else if (no_fat_chain) {
            if (curr_cluster != prev_cluster + 1) {
                no_fat_chain = 0;
                for (uint32_t c = first_cluster; c < prev_cluster; c++) {
                    set_fat_entry(vol, c, c + 1);
                }
                set_fat_entry(vol, prev_cluster, curr_cluster);
                set_fat_entry(vol, curr_cluster, 0xFFFFFFFF);
            }
        } else {
            set_fat_entry(vol, prev_cluster, curr_cluster);
            set_fat_entry(vol, curr_cluster, 0xFFFFFFFF);
        }

        uint64_t data_sec = cluster_to_sectors(vol, curr_cluster);
        for (uint32_t s = 0; s < vol->sectors_per_cluster && bytes_written < size; s++) {
            uint8_t data_pad[512] = {0};
            uint64_t rem = size - bytes_written;
            uint32_t chunk = (rem > 512) ? 512 : (uint32_t)rem;

            memcpy(data_pad, (const uint8_t *)data + bytes_written, chunk);
            if (write_offset(vol, data_sec + s, data_pad, 1) != 0) return -4;
            bytes_written += chunk;
        }
        prev_cluster = curr_cluster;
    }

    se.first_cluster = first_cluster;
    se.valid_data_length = size;
    se.data_length = size;
    se.secondary_flags = 0x01 | (no_fat_chain ? 0x02 : 0x00);

    uint8_t window[WINDOW_SIZE];
    uint64_t base_sector = cluster_to_sectors(vol, cluster);
    uint32_t sec = slot_idx / 16;
    uint32_t slot = slot_idx % 16;

    if (read_offset(vol, base_sector + sec, window, 1) != 0) return -1;
    memcpy(&window[slot * 32 + 32], &se, sizeof(stream_entry));
    if (write_offset(vol, base_sector + sec, window, 1) != 0) return -4;

    return 0;
}

int read_file(mount_t *vol, const char *path, uint8_t *out_buf, uint64_t max_size, uint64_t *bytes_read_out) {
    char full[256];
    build_path(path, full);

    char parent_path[256];
    char file_name[256];

    split_path(full, parent_path, file_name);

    uint32_t parent_cluster = resolve_path(vol, parent_path);
    if (parent_cluster == 0) return -2;

    file_entry fe;
    stream_entry se;
    if (find_entry_in_dir(vol, parent_cluster, file_name, &fe, &se, NULL, NULL) != 0) {
        return -2;
    }

    uint64_t to_read = (se.valid_data_length < max_size) ? se.valid_data_length : max_size;
    uint64_t bytes_read = 0;
    uint32_t curr_cluster = se.first_cluster;
    uint8_t no_fat_chain = se.secondary_flags & 0x02;
    uint8_t sec_buf[SECTOR_SIZE];
    uint8_t fat_buf[SECTOR_SIZE];

    while (bytes_read < to_read && curr_cluster >= 2 && curr_cluster < 0xFFFFFFF8) {
        uint64_t data_sec = cluster_to_sectors(vol, curr_cluster);
        for (uint32_t s = 0; s < vol->sectors_per_cluster && bytes_read < to_read; s++) {
            if (read_offset(vol, data_sec + s, sec_buf, 1) != 0) return -3;
            uint64_t rem = to_read - bytes_read;
            uint32_t chunk = (rem > SECTOR_SIZE) ? SECTOR_SIZE : (uint32_t)rem;
            memcpy((uint8_t *)out_buf + bytes_read, sec_buf, chunk);
            bytes_read += chunk;
        }
        if (no_fat_chain) {
            curr_cluster++;
        } else {
            curr_cluster = next_cluster(vol, curr_cluster, fat_buf);
        }
    }

    if (bytes_read_out) *bytes_read_out = bytes_read;
    return 0;
}

int append_file(mount_t *vol, const char *path, const uint8_t *data, uint64_t size) {
    char full[256];
    build_path(path, full);

    char parent_path[256];
    char file_name[256];

    split_path(full, parent_path, file_name);

    uint32_t parent_cluster = resolve_path(vol, parent_path);
    if (parent_cluster == 0) return -2;

    file_entry fe;
    stream_entry se;
    uint32_t slot_idx, cluster;
    if (find_entry_in_dir(vol, parent_cluster, file_name, &fe, &se, &slot_idx, &cluster) != 0) {
        return -2;
    }

    uint64_t old_size      = se.valid_data_length;
    uint32_t first_cluster = se.first_cluster;
    uint8_t  no_fat_chain  = (se.secondary_flags & 0x02) ? 1 : 0;
    uint32_t last_cluster  = 0;
    uint64_t bytes_written = 0;
    uint8_t sec_buf[SECTOR_SIZE];
    uint8_t fat_buf[SECTOR_SIZE];

    if (first_cluster >= 2 && old_size > 0) {
        uint32_t num_clusters = (old_size + vol->bytes_per_cluster - 1) / vol->bytes_per_cluster;

        if (no_fat_chain) {
            last_cluster = first_cluster + num_clusters - 1;
        } else {
            last_cluster = first_cluster;
            for (uint32_t c = 1; c < num_clusters; c++) {
                uint32_t next = next_cluster(vol, last_cluster, fat_buf);
                if (next < 2 || next >= 0xFFFFFFF8) break;
                last_cluster = next;
            }
        }

        uint32_t offset_in_cluster = old_size % vol->bytes_per_cluster;
        if (offset_in_cluster > 0) {
            uint64_t data_sec      = cluster_to_sectors(vol, last_cluster);
            uint32_t start_sec     = offset_in_cluster / SECTOR_SIZE;
            uint32_t offset_in_sec = offset_in_cluster % SECTOR_SIZE;

            for (uint32_t s = start_sec; s < vol->sectors_per_cluster && bytes_written < size; s++) {
                if (read_offset(vol, data_sec + s, sec_buf, 1) != 0) return -3;

                uint32_t space = SECTOR_SIZE - offset_in_sec;
                uint64_t rem   = size - bytes_written;
                uint32_t chunk = (rem < space) ? (uint32_t)rem : space;

                memcpy(sec_buf + offset_in_sec, (const uint8_t *)data + bytes_written, chunk);
                if (write_offset(vol, data_sec + s, sec_buf, 1) != 0) return -4;

                bytes_written += chunk;
                offset_in_sec = 0;
            }
        }
    }

    while (bytes_written < size) {
        uint32_t curr_cluster = alloc_cluster_near(vol, last_cluster + 1);
        if (curr_cluster < 2) return -3;

        if (first_cluster == 0) {
            first_cluster = curr_cluster;
            no_fat_chain  = 1;
        } else if (no_fat_chain) {
            if (curr_cluster != last_cluster + 1) {
                no_fat_chain = 0;
                for (uint32_t c = first_cluster; c < last_cluster; c++) {
                    set_fat_entry(vol, c, c + 1);
                }
                set_fat_entry(vol, last_cluster, curr_cluster);
                set_fat_entry(vol, curr_cluster, 0xFFFFFFFF);
            }
        } else {
            set_fat_entry(vol, last_cluster, curr_cluster);
            set_fat_entry(vol, curr_cluster, 0xFFFFFFFF);
        }

        uint64_t data_sec = cluster_to_sectors(vol, curr_cluster);
        for (uint32_t s = 0; s < vol->sectors_per_cluster && bytes_written < size; s++) {
            memset(sec_buf, 0, SECTOR_SIZE);
            uint64_t rem   = size - bytes_written;
            uint32_t chunk = (rem > SECTOR_SIZE) ? SECTOR_SIZE : (uint32_t)rem;

            memcpy(sec_buf, (const uint8_t *)data + bytes_written, chunk);
            if (write_offset(vol, data_sec + s, sec_buf, 1) != 0) return -4;
            bytes_written += chunk;
        }

        last_cluster = curr_cluster;
    }

    se.first_cluster     = first_cluster;
    se.valid_data_length = old_size + size;
    se.data_length       = old_size + size;
    se.secondary_flags   = 0x01 | (no_fat_chain ? 0x02 : 0x00);

    uint8_t window[WINDOW_SIZE];
    uint64_t base_sector = cluster_to_sectors(vol, cluster);
    uint32_t sec = slot_idx / 16;
    uint32_t slot = slot_idx % 16;

    if (read_offset(vol, base_sector + sec, window, 1) != 0) return -1;
    memcpy(&window[slot * 32 + 32], &se, sizeof(stream_entry));
    if (write_offset(vol, base_sector + sec, window, 1) != 0) return -4;

    return 0;
}

int list_dir(mount_t *vol,
             const char *path,
             fs_t *out_files,
             int max_files)
{
    if (!vol || !out_files || max_files <= 0)
        return -1;

    char full[256];

    if (!path || path[0] == '\0')
        strcpy(full, g_cwd);
    else
        build_path(path, full);

    uint32_t target_cluster =
        resolve_path(vol, full);

    if (target_cluster == 0)
        return -2;

    uint8_t window[WINDOW_SIZE];
    uint8_t fat_buf[SECTOR_SIZE];

    int count = 0;
    uint32_t curr_cluster = target_cluster;

    while (curr_cluster >= 2 &&
           curr_cluster < 0xFFFFFFF8)
    {
        uint64_t dir_sector =
            cluster_to_sectors(vol,
                               curr_cluster);

        uint32_t dir_sectors =
            vol->bytes_per_cluster /
            SECTOR_SIZE;

        for (uint32_t sec = 0;
             sec < dir_sectors;
             sec++)
        {
            if (read_offset(vol,
                            dir_sector + sec,
                            window,
                            1) != 0)
                return -1;

            if (sec + 1 < dir_sectors) {
                if (read_offset(vol,
                                dir_sector + sec + 1,
                                window + SECTOR_SIZE,
                                1) != 0)
                    return -1;
            } else {
                memset(window + SECTOR_SIZE,
                       0,
                       SECTOR_SIZE);
            }

            for (uint32_t slot = 0;
                 slot < 16;
                 slot++)
            {
                uint8_t *ptr =
                    window + (slot * 32);

                if (*ptr == 0x00)
                    return count;

                if (*ptr != 0x85)
                    continue;

                file_entry *fe =
                    (file_entry *)ptr;

                stream_entry *se =
                    (stream_entry *)(ptr + 32);

                if (se->entry_type != 0xC0)
                    continue;

                fs_t *item =
                    &out_files[count];

                memset(item, 0, sizeof(fs_t));

                item->is_dir = (fe->attribute & 0x0010) ? true : false;

                item->size = (uint32_t)se->valid_data_length;

                size_t len = se->name_length;

                if (len > 255)
                    len = 255;

                for (size_t i = 0; i < len; i++) {
                    file_name_entry *fn = (file_name_entry *)(ptr + (2 + (i / 15)) * 32);

                    uint16_t *utf16 = (uint16_t *)fn->filename;

                    item->name[i] = (char)(utf16[i % 15] & 0xFF);
                }

                item->name[len] = '\0';

                count++;

                if (count >= max_files)
                    return count;
            }
        }

        curr_cluster = next_cluster(vol, curr_cluster, fat_buf);
    }

    return count;
}

int delete_file(mount_t *vol, const char *path) {
    char full[256];
    build_path(path, full);

    char parent_path[256];
    char file_name[256];

    split_path(full, parent_path, file_name);

    uint32_t parent_cluster = resolve_path(vol, parent_path);
    if (parent_cluster == 0) return -2;

    size_t name_len = strlen(file_name);
    if (name_len == 0 || name_len > 255) return -1;

    uint8_t window[WINDOW_SIZE];
    uint8_t fat_buf[SECTOR_SIZE];
    uint32_t curr = parent_cluster;

    while (curr >= 2 && curr < 0xFFFFFFF8) {
        uint64_t dir_sector = cluster_to_sectors(vol, curr);
        uint32_t dir_sectors = vol->bytes_per_cluster / SECTOR_SIZE;

        for (uint32_t sec = 0; sec < dir_sectors; sec++) {
            uint64_t current_sec_lba = dir_sector + sec;
            if (read_offset(vol, current_sec_lba, window, 1) != 0) return -1;
            
            if (sec + 1 < dir_sectors) {
                if (read_offset(vol, dir_sector + sec + 1, window + SECTOR_SIZE, 1) != 0) return -1;
            } else {
                memset(window + SECTOR_SIZE, 0, SECTOR_SIZE);
            }

            for (uint32_t slot = 0; slot < 16; slot++) {
                uint8_t *ptr = &window[slot * 32];
                if (*ptr == 0x00) return -2;
                if (*ptr != 0x85) continue;  // not a file entry
                
                file_entry   *fe = (file_entry *)ptr;
                stream_entry *se = (stream_entry *)(ptr + 32);
                if (se->entry_type != 0xC0 || se->name_length != name_len) continue;

                // verify if the filenames match
                uint8_t match = 1;
                for (size_t i = 0; i < name_len; i++) {
                    file_name_entry *fn = (file_name_entry *)(ptr + (2 + (i / 15)) * 32);
                    uint16_t *utf16 = (uint16_t *)fn->filename;
                    if (utf16[i % 15] != (uint8_t)file_name[i]) {
                        match = 0;
                        break;
                    }
                }

                if (!match) continue;

                uint8_t total_entries = 1 + fe->scount;
                free_cluster_chain(vol, se->first_cluster, se->data_length, se->secondary_flags & 0x02);

                for (uint8_t e = 0; e < total_entries; e++) {
                    uint32_t target_slot = slot + e;
                    if (target_slot < 16) {
                        window[target_slot * 32] &= ~0x80; 
                    } else {
                        window[(target_slot - 16) * 32 + SECTOR_SIZE] &= ~0x80;
                    }
                }

                // write updated sectors
                if (write_offset(vol, current_sec_lba, window, 1) != 0) return -4;
                
                if ((slot + total_entries > 16) && (sec + 1 < dir_sectors)) {
                    if (write_offset(vol, dir_sector + sec + 1, window + SECTOR_SIZE, 1) != 0) return -4;
                }

                return 0; // Successfully deleted
            }
        }
        curr = next_cluster(vol, curr, fat_buf);
    }

    return -2; // File not found
}

int change_dir(mount_t *vol, const char *path) {
    char full[256];

    build_path(path, full);

    uint32_t cluster = resolve_path(vol, full);
    if (cluster == 0)
        return -1;

    strcpy(g_cwd, full);

    size_t len = strlen(g_cwd);
    while (len > 1 && g_cwd[len - 1] == '/') {
        g_cwd[len - 1] = '\0';
        len--;
    }

    return 0;
}

void get_cwd(char* buffer, int size) {
    strncpy(buffer, g_cwd, size);
}