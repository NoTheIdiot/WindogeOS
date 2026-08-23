#include <boot/kernel.h>
#include <boot/limine.h>
#include <dogeio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <basicutil.h>
#include <bool.h>

#define ATA_STATUS     0x1F7
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LOW    0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HIGH   0x1F5
#define ATA_COMMAND    0x1F7
#define ATA_DATA       0x1F0

#define EXFAT_PARTITION_LBA 4096
#define EXFAT_TOTAL_SECTORS 2097152

#define EXFAT_TYPE_BITMAP 0x81
#define EXFAT_TYPE_UPCASE 0x82
#define EXFAT_TYPE_FILE   0x85
#define EXFAT_TYPE_STREAM 0xC0
#define EXFAT_TYPE_NAME   0xC1

typedef struct __attribute__((packed)) {
    uint8_t  jump_boot[3];
    uint8_t  fs_name[8];
    uint8_t  zero[53];
    uint64_t partition_offset;
    uint64_t volume_length;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t root_dir_cluster;
    uint32_t volume_serial;
    uint16_t fs_revision;
    uint16_t volume_flags;
    uint8_t  bytes_per_sector_shift;
    uint8_t  sectors_per_cluster_shift;
    uint8_t  num_fats;
    uint8_t  drive_select;
    uint8_t  percent_in_use;
    uint8_t  reserved[7];
    uint8_t  boot_code[390];
    uint16_t boot_signature;
} exfat_header_t;

typedef struct __attribute__((packed)) {
    uint8_t  entry_type;
    uint8_t  secondary_count;
    uint16_t set_checksum;
    uint16_t file_attributes;
    uint16_t reserved1;
    uint32_t create_time;
    uint32_t last_mod_time;
    uint32_t last_access_time;
    uint8_t  create_10ms;
    uint8_t  last_mod_10ms;
    uint8_t  create_tz;
    uint8_t  last_mod_tz;
    uint8_t  last_access_tz;
    uint8_t  reserved2[7];
} exfat_dentry_file_t;

typedef struct __attribute__((packed)) {
    uint8_t  entry_type;
    uint8_t  flags;
    uint8_t  reserved1;
    uint8_t  name_length;
    uint16_t name_hash;
    uint16_t reserved2;
    uint64_t valid_data_length;
    uint32_t reserved3;
    uint32_t first_cluster;
    uint64_t data_length;
} exfat_dentry_stream_t;

typedef struct __attribute__((packed)) {
    uint8_t  entry_type;
    uint8_t  flags;
    uint16_t unicode_name[15];
} exfat_dentry_name_t;

typedef struct {
    uint32_t cluster;
    uint64_t size;
    bool     is_dir;
    uint64_t entry_lba;
    uint64_t entry_offset;
} exfat_target_t;

static uint32_t g_current_cluster = 4;
static char     g_current_path[256] = "/";
static uint32_t g_cluster_heap_offset = 2048;
static uint32_t g_sectors_per_cluster = 8;
static uint32_t g_root_cluster         = 4;

static const uint8_t exfat_upcase_default[128] = {
    0x61, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x41, 0x00
};

static void exfat_num_to_str(uint64_t num, char *out) {
    if (num == 0) { out[0] = '0'; out[1] = '\0'; return; }
    char tmp[32]; int i = 0;
    while (num > 0) { tmp[i++] = '0' + (char)(num % 10); num /= 10; }
    int j = 0;
    while (i > 0) { out[j++] = tmp[--i]; }
    out[j] = '\0';
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

static inline uint64_t exfat_cluster_lba(uint32_t cluster) {
    return EXFAT_PARTITION_LBA + g_cluster_heap_offset + ((uint64_t)(cluster - 2) * g_sectors_per_cluster);
}

static inline int exfat_hw_ready(void) {
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);
    uint8_t status;
    do {
        status = ports_inb(ATA_STATUS);
        if (status & 0x21) {
            log("fs (err) ATA status hardware fault!");
            return -1;
        }
    } while ((status & 0x80) || !(status & 0x08));
    return 0;
}

static int exfat_sector_read(uint64_t lba, uint8_t *buffer) {
    if (lba > 0x000FFFFFFFFFFFFULL) {
        log("fs (err) Read LBA out of 48-bit bounds");
        return -1;
    }

    ports_outb(ATA_DRIVE_HEAD, 0x40);
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 0);
    ports_outb(ATA_LBA_LOW,  (uint8_t)(lba >> 24));
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 32));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 40));

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND,  0x24);

    if (exfat_hw_ready() != 0) return -1;

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) ptr[i] = ports_inw(ATA_DATA);
    return 0;
}

static int exfat_sector_write(uint64_t lba, const uint8_t *buffer) {
    if (lba > 0x000FFFFFFFFFFFFULL) {
        log("fs (err) Write LBA out of 48-bit bounds");
        return -1;
    }

    ports_outb(ATA_DRIVE_HEAD, 0x40);
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 0);
    ports_outb(ATA_LBA_LOW,  (uint8_t)(lba >> 24));
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 32));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 40));

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND,  0x34);

    if (exfat_hw_ready() != 0) return -1;

    const uint16_t *ptr = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) ports_outw(ATA_DATA, ptr[i]);

    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);
    while (ports_inb(ATA_STATUS) & 0x80);

    ports_outb(ATA_COMMAND, 0xE7);
    uint8_t status;
    do {
        status = ports_inb(ATA_STATUS);
        if (status & 0x21) {
            log("fs (err) Cache flush failed after write");
            return -1;
        }
    } while (status & 0x80);

    return 0;
}

static uint32_t exfat_calc_boot_crc(const uint8_t *sector, size_t bytes, uint32_t crc, bool is_vbr) {
    for (size_t i = 0; i < bytes; i++) {
        if (is_vbr && (i == 106 || i == 107 || i == 112)) continue;
        crc = ((crc & 1) ? 0x80000000 : 0) + (crc >> 1) + (uint32_t)sector[i];
    }
    return crc;
}

static uint16_t exfat_calc_entry_crc(const uint8_t *entries, uint8_t count) {
    uint16_t crc = 0;
    size_t len = (size_t)count * 32;
    for (size_t i = 0; i < len; i++) {
        if (i == 2 || i == 3) continue;
        crc = ((crc & 1) ? 0x8000 : 0) + (crc >> 1) + (uint16_t)entries[i];
    }
    return crc;
}

static uint16_t exfat_calc_name_hash(const uint16_t *name, uint8_t len) {
    uint16_t hash = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint16_t ch = name[i];
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        hash = ((hash & 1) ? 0x8000 : 0) + (hash >> 1) + (uint16_t)(ch & 0xFF);
        hash = ((hash & 1) ? 0x8000 : 0) + (hash >> 1) + (uint16_t)((ch >> 8) & 0xFF);
    }
    return hash;
}

static uint32_t exfat_alloc_blocks(uint32_t count) {
    if (count == 0) return 0;
    uint8_t sector[512];
    uint64_t bitmap_base_lba = exfat_cluster_lba(2);
    uint32_t total_clusters = (EXFAT_TOTAL_SECTORS - 2048) / 8;
    uint32_t bitmap_sectors = (total_clusters + 4095) / 4096;

    uint32_t consecutive = 0;
    uint32_t start_cluster = 0;

    for (uint32_t s = 0; s < bitmap_sectors; s++) {
        if (exfat_sector_read(bitmap_base_lba + s, sector) != 0) {
            log("fs (err) alloc_blocks failed reading bitmap sector");
            return 0;
        }

        for (uint32_t bit = 0; bit < 4096; bit++) {
            uint32_t current_idx = s * 4096 + bit;
            if (current_idx >= total_clusters) break;

            uint32_t byte_idx = bit / 8;
            uint8_t bit_idx = bit % 8;

            if (!(sector[byte_idx] & (1 << bit_idx))) {
                if (consecutive == 0) start_cluster = current_idx;
                consecutive++;
                if (consecutive == count) {
                    for (uint32_t i = start_cluster; i < start_cluster + count; i++) {
                        uint32_t target_sec = i / 4096;
                        uint32_t target_bit = i % 4096;

                        uint8_t sec_buf[512];
                        if (exfat_sector_read(bitmap_base_lba + target_sec, sec_buf) != 0) return 0;

                        sec_buf[target_bit / 8] |= (1 << (target_bit % 8));
                        if (exfat_sector_write(bitmap_base_lba + target_sec, sec_buf) != 0) return 0;
                    }
                    return start_cluster + 2;
                }
            } else {
                consecutive = 0;
            }
        }
    }
    log("fs (err) Bitmap full or no contiguous space large enough");
    return 0;
}

static uint32_t exfat_alloc_block(void) {
    return exfat_alloc_blocks(1);
}

static void exfat_free_blocks(uint32_t cluster, uint32_t count) {
    if (cluster < 2 || count == 0) return;
    uint64_t bitmap_base_lba = exfat_cluster_lba(2);

    uint32_t start_idx = cluster - 2;
    for (uint32_t i = start_idx; i < start_idx + count; i++) {
        uint32_t sec_idx = i / 4096;
        uint32_t bit_idx = i % 4096;

        uint8_t sector[512];
        if (exfat_sector_read(bitmap_base_lba + sec_idx, sector) == 0) {
            sector[bit_idx / 8] &= ~(1 << (bit_idx % 8));
            exfat_sector_write(bitmap_base_lba + sec_idx, sector);
        }
    }
}

static void exfat_free_block(uint32_t cluster) {
    exfat_free_blocks(cluster, 1);
}

int exfat_resolve_entry(const char *target_name, exfat_target_t *out) {
    uint8_t cluster_buf[4096];
    uint64_t base_lba = exfat_cluster_lba(g_current_cluster);

    for (uint32_t s = 0; s < 8; s++) {
        if (exfat_sector_read(base_lba + s, cluster_buf + (s * 512)) != 0) return -1;
    }

    for (int i = 0; i <= 4096 - 96; i += 32) {
        if (cluster_buf[i] == EXFAT_TYPE_FILE) {
            exfat_dentry_file_t *file = (exfat_dentry_file_t *)&cluster_buf[i];
            exfat_dentry_stream_t *stream = (exfat_dentry_stream_t *)&cluster_buf[i + 32];

            char name_buf[256] = {0};
            int pos = 0;

            for (uint8_t sec = 2; sec <= file->secondary_count; sec++) {
                if (i + (sec * 32) >= 4096) break;
                exfat_dentry_name_t *name_ent = (exfat_dentry_name_t *)&cluster_buf[i + (sec * 32)];
                if (name_ent->entry_type == EXFAT_TYPE_NAME) {
                    for (int c = 0; c < 15; c++) {
                        uint16_t ch = name_ent->unicode_name[c];
                        if (ch == 0) break;
                        if (pos < 255) name_buf[pos++] = (char)ch;
                    }
                }
            }

            if (str_strcmp(name_buf, target_name) == 0) {
                if (out) {
                    out->entry_lba = base_lba + ((uint64_t)i / 512);
                    out->entry_offset = (uint64_t)(i % 512);
                    out->cluster = stream->first_cluster;
                    out->size = stream->data_length;
                    out->is_dir = (file->file_attributes & 0x10) != 0;
                }
                return 0;
            }
            i += file->secondary_count * 32;
        }
    }
    return -1;
}
#pragma clang diagnostic pop

int exfat_mount(void) {
    uint8_t sector[512];
    if (exfat_sector_read(EXFAT_PARTITION_LBA, sector) != 0) {
        log("fs (err) Failed to read exFAT VBR");
        return -1;
    }

    exfat_header_t *vbr = (exfat_header_t *)sector;

    if (memcmp(vbr->fs_name, "EXFAT   ", 8) != 0) {
        log("fs (err) Invalid exFAT magic signature");
        return -1;
    }

    g_cluster_heap_offset = vbr->cluster_heap_offset;
    g_sectors_per_cluster = 1 << vbr->sectors_per_cluster_shift;
    g_root_cluster        = vbr->root_dir_cluster;

    g_current_cluster = g_root_cluster;
    str_strcpy(g_current_path, "/");

    log("fs (log) exFAT mounted successfully");
    return 1;
}

int exfat_wipe_and_format(void) {
    log("fs (log) Formatting volume...");
    uint8_t sector[512];
    memset(sector, 0, 512);

    uint32_t heap_offset = 2048;
    uint32_t cluster_cnt = (EXFAT_TOTAL_SECTORS - heap_offset) / 8;
    uint32_t fat_len = ((cluster_cnt + 2) * 4 + 511) / 512;

    exfat_header_t *vbr = (exfat_header_t *)sector;
    vbr->jump_boot[0] = 0xEB; vbr->jump_boot[1] = 0x76; vbr->jump_boot[2] = 0x90;
    memcpy(vbr->fs_name, "EXFAT   ", 8);
    vbr->partition_offset = EXFAT_PARTITION_LBA;
    vbr->volume_length = EXFAT_TOTAL_SECTORS;
    vbr->fat_offset = 128;
    vbr->fat_length = fat_len;
    vbr->cluster_heap_offset = heap_offset;
    vbr->cluster_count = cluster_cnt;
    vbr->root_dir_cluster = 4;
    vbr->volume_serial = 0x87654321;
    vbr->fs_revision = 0x0100;
    vbr->bytes_per_sector_shift = 9;
    vbr->sectors_per_cluster_shift = 3;
    vbr->num_fats = 1;
    vbr->drive_select = 0x80;
    vbr->boot_signature = 0xAA55;

    uint32_t crc = exfat_calc_boot_crc(sector, 512, 0, true);
    if (exfat_sector_write(EXFAT_PARTITION_LBA + 0, sector) != 0) return -1;
    exfat_sector_write(EXFAT_PARTITION_LBA + 12, sector);

    uint8_t zero_sector[512] = {0};
    for (int i = 1; i <= 10; i++) {
        crc = exfat_calc_boot_crc(zero_sector, 512, crc, false);
        exfat_sector_write(EXFAT_PARTITION_LBA + (uint64_t)i, zero_sector);
        exfat_sector_write(EXFAT_PARTITION_LBA + (uint64_t)12 + (uint64_t)i, zero_sector);
    }

    uint32_t crc_table[128];
    for (int i = 0; i < 128; i++) crc_table[i] = crc;
    exfat_sector_write(EXFAT_PARTITION_LBA + 11, (uint8_t*)crc_table);
    exfat_sector_write(EXFAT_PARTITION_LBA + 23, (uint8_t*)crc_table);

    memset(sector, 0, 512);
    uint32_t *fat = (uint32_t *)sector; // PRESERVED: exFAT FAT entries are strictly 32-bit
    fat[0] = 0xFFFFFFF8; fat[1] = 0xFFFFFFFF;
    fat[2] = 0xFFFFFFFF; fat[3] = 0xFFFFFFFF; fat[4] = 0xFFFFFFFF;
    exfat_sector_write(EXFAT_PARTITION_LBA + 128, sector);

    memset(sector, 0, 512); sector[0] = 0x07;
    exfat_sector_write(EXFAT_PARTITION_LBA + heap_offset + (0 * 8), sector);

    memset(sector, 0, 512);
    memcpy(sector, exfat_upcase_default, sizeof(exfat_upcase_default));
    exfat_sector_write(EXFAT_PARTITION_LBA + heap_offset + (1 * 8), sector);

    memset(sector, 0, 512);
    sector[0] = EXFAT_TYPE_BITMAP;
    *(uint32_t *)&sector[20] = 2;
    *(uint64_t *)&sector[24] = (cluster_cnt + 7) / 8;

    sector[32] = EXFAT_TYPE_UPCASE;
    *(uint32_t *)&sector[32 + 4] = 0xE6163351;
    *(uint32_t *)&sector[32 + 20] = 3;
    *(uint64_t *)&sector[32 + 24] = sizeof(exfat_upcase_default);

    exfat_sector_write(EXFAT_PARTITION_LBA + heap_offset + (2 * 8), sector);

    g_current_cluster = 4;
    str_strcpy(g_current_path, "/");
    log("fs (log) Format complete.");
    return 0;
}

int exfat_create_node(const char *name, bool is_dir) {
    if (!name || name[0] == '\0') return -1;

    uint8_t cluster_buf[4096];
    uint64_t base_lba = exfat_cluster_lba(g_current_cluster);

    for (uint32_t s = 0; s < 8; s++) {
        if (exfat_sector_read(base_lba + s, cluster_buf + (s * 512)) != 0) return -1;
    }

    uint32_t len = (uint32_t)str_strlen(name);
    if (len > 255) len = 255;

    uint8_t name_entries = (uint8_t)((len + 14) / 15);
    uint8_t secondary_count = 1 + name_entries;
    uint32_t total_entries = 1 + secondary_count;
    uint32_t needed_bytes = total_entries * 32;

    int slot = -1;
    for (int i = 0; i <= (int)(4096 - needed_bytes); i += 32) {
        bool fits = true;
        for (uint32_t j = 0; j < needed_bytes; j += 32) {
            uint8_t type = cluster_buf[(uint32_t)i + j];
            if ((type & 0x80) != 0 && type != 0x00) {
                fits = false;
                break;
            }
        }
        if (fits) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    uint32_t new_cluster = exfat_alloc_block();
    if (new_cluster == 0) return -1;

    if (is_dir) {
        uint8_t zero_buf[512] = {0};
        uint64_t dir_lba = exfat_cluster_lba(new_cluster);
        for (uint32_t s = 0; s < 8; s++) {
            if (exfat_sector_write(dir_lba + s, zero_buf) != 0) return -1;
        }
    }

    memset(&cluster_buf[slot], 0, needed_bytes);

    exfat_dentry_file_t *file = (exfat_dentry_file_t *)&cluster_buf[slot];
    file->entry_type = EXFAT_TYPE_FILE;
    file->secondary_count = secondary_count;
    file->file_attributes = is_dir ? 0x10 : 0x20;

    exfat_dentry_stream_t *stream = (exfat_dentry_stream_t *)&cluster_buf[slot + 32];
    stream->entry_type = EXFAT_TYPE_STREAM;
    stream->flags = 0x03;
    stream->name_length = (uint8_t)len;
    stream->first_cluster = new_cluster;

    uint16_t unicode_buf[256] = {0};
    for (uint32_t i = 0; i < len; i++) {
        unicode_buf[i] = (uint16_t)name[i];
    }
    stream->name_hash = exfat_calc_name_hash(unicode_buf, (uint8_t)len);

    uint32_t chars_left = len;
    for (uint8_t entry_idx = 0; entry_idx < name_entries; entry_idx++) {
        exfat_dentry_name_t *fname = (exfat_dentry_name_t *)&cluster_buf[slot + 64 + (entry_idx * 32)];
        fname->entry_type = EXFAT_TYPE_NAME;

        uint8_t chunk = (chars_left > 15) ? 15 : (uint8_t)chars_left;
        for (uint8_t i = 0; i < chunk; i++) {
            fname->unicode_name[i] = unicode_buf[(entry_idx * 15) + i];
        }
        chars_left -= chunk;
    }

    file->set_checksum = exfat_calc_entry_crc(&cluster_buf[slot], secondary_count + 1);

    uint32_t start_sec = (uint32_t)slot / 512;
    uint32_t end_sec = ((uint32_t)slot + needed_bytes - 1) / 512;
    for (uint32_t s = start_sec; s <= end_sec; s++) {
        if (exfat_sector_write(base_lba + s, cluster_buf + (s * 512)) != 0) return -1;
    }

    return 0;
}

int exfat_write_file(const char *name, const uint8_t *data, uint64_t count) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0) return -1;

    uint32_t old_clusters = (target.size == 0) ? 1 : (uint32_t)((target.size + 4095) / 4096);
    uint32_t new_clusters = (count == 0) ? 1 : (uint32_t)((count + 4095) / 4096);
    uint32_t active_cluster = target.cluster;

    if (new_clusters != old_clusters) {
        uint32_t allocated = exfat_alloc_blocks(new_clusters);
        if (allocated == 0) return -1;
        exfat_free_blocks(target.cluster, old_clusters);
        active_cluster = allocated;
    }

    uint8_t sector[512];
    uint64_t base_lba = exfat_cluster_lba(active_cluster);
    uint64_t sectors_needed = (count + 511) / 512;

    for (uint64_t i = 0; i < sectors_needed; i++) {
        memset(sector, 0, 512);
        uint64_t chunk = (count - (i * 512) > 512) ? 512 : (count - (i * 512));
        memcpy(sector, data + (i * 512), chunk);
        if (exfat_sector_write(base_lba + i, sector) != 0) return -1;
    }

    if (exfat_sector_read(target.entry_lba, sector) != 0) return -1;

    exfat_dentry_file_t *file = (exfat_dentry_file_t *)&sector[target.entry_offset];
    exfat_dentry_stream_t *stream = (exfat_dentry_stream_t *)&sector[target.entry_offset + 32];

    stream->first_cluster = active_cluster;
    stream->data_length = count;
    stream->valid_data_length = count;
    file->set_checksum = exfat_calc_entry_crc(&sector[target.entry_offset], file->secondary_count + 1);

    return exfat_sector_write(target.entry_lba, sector);
}

int exfat_append_file(const char *name, const uint8_t *data, uint64_t count) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0) return -1;
    if (count == 0) return 0;

    uint64_t new_total = target.size + count;
    uint32_t old_clusters = (target.size == 0) ? 1 : (uint32_t)((target.size + 4095) / 4096);
    uint32_t new_clusters = (uint32_t)((new_total + 4095) / 4096);
    uint32_t active_cluster = target.cluster;

    if (new_clusters > old_clusters) {
        uint32_t allocated = exfat_alloc_blocks(new_clusters);
        if (allocated == 0) return -1;

        uint8_t temp[512];
        uint64_t old_lba = exfat_cluster_lba(target.cluster);
        uint64_t new_lba = exfat_cluster_lba(allocated);
        uint64_t old_sectors = (target.size + 511) / 512;

        for (uint64_t s = 0; s < old_sectors; s++) {
            if (exfat_sector_read(old_lba + s, temp) != 0 ||
                exfat_sector_write(new_lba + s, temp) != 0) return -1;
        }

        exfat_free_blocks(target.cluster, old_clusters);
        active_cluster = allocated;
    }

    uint64_t start_lba = exfat_cluster_lba(active_cluster);
    uint64_t current_offset = target.size;
    uint64_t remaining = count;
    const uint8_t *src = data;

    while (remaining > 0) {
        uint64_t sec_index = current_offset / 512;
        uint32_t sec_offset = (uint32_t)(current_offset % 512);
        uint32_t write_len = 512 - sec_offset;
        if (write_len > remaining) write_len = (uint32_t)remaining;

        uint8_t temp[512];
        memset(temp, 0, 512);

        if (sec_offset > 0) {
            if (exfat_sector_read(start_lba + sec_index, temp) != 0) return -1;
        }

        memcpy(temp + sec_offset, src, write_len);
        if (exfat_sector_write(start_lba + sec_index, temp) != 0) return -1;

        current_offset += write_len;
        src += write_len;
        remaining -= write_len;
    }

    uint8_t sector[512];
    if (exfat_sector_read(target.entry_lba, sector) != 0) return -1;

    exfat_dentry_file_t *file = (exfat_dentry_file_t *)&sector[target.entry_offset];
    exfat_dentry_stream_t *stream = (exfat_dentry_stream_t *)&sector[target.entry_offset + 32];

    stream->first_cluster = active_cluster;
    stream->data_length = new_total;
    stream->valid_data_length = new_total;
    file->set_checksum = exfat_calc_entry_crc(&sector[target.entry_offset], file->secondary_count + 1);

    return exfat_sector_write(target.entry_lba, sector);
}

// UPDATED: Return type upgraded to int64_t to prevent signed 32-bit overflow on files >2 GiB
int64_t exfat_read_file(const char *name, uint8_t *out_buf, uint64_t max_bytes) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0) return -1;

    uint8_t sector[512];
    uint64_t base_lba = exfat_cluster_lba(target.cluster);
    uint64_t bytes_to_read = (target.size < max_bytes) ? target.size : max_bytes;

    uint8_t *dst = out_buf;
    uint64_t left = bytes_to_read;
    uint64_t sector_idx = 0;

    while (left > 0) {
        if (exfat_sector_read(base_lba + sector_idx, sector) != 0) return -1;
        uint32_t chunk = (left > 512) ? 512 : (uint32_t)left;
        memcpy(dst, sector, chunk);
        dst += chunk;
        left -= chunk;
        sector_idx++;
    }

    char numbuf[32];
    exfat_num_to_str(bytes_to_read, numbuf);
    char log_buf[64] = "fs (log) Read ";
    str_strcat(log_buf, numbuf);
    str_strcat(log_buf, " bytes successfully");
    log(log_buf);

    return (int64_t)bytes_to_read;
}

int exfat_delete_node(const char *name) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0) return -1;

    uint32_t clusters = (target.size == 0) ? 1 : (uint32_t)((target.size + 4095) / 4096);
    exfat_free_blocks(target.cluster, clusters);

    uint8_t sector[512];
    if (exfat_sector_read(target.entry_lba, sector) != 0) return -1;

    exfat_dentry_file_t *file = (exfat_dentry_file_t *)&sector[target.entry_offset];
    uint8_t total = file->secondary_count + 1;

    for (uint8_t i = 0; i < total; i++) {
        sector[target.entry_offset + (i * 32)] &= 0x7F;
    }

    return exfat_sector_write(target.entry_lba, sector);
}

int exfat_truncate_last_line(const char *name) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0 || target.size == 0) return -1;

    uint64_t last_sector_index = (target.size - 1) / 512;
    uint64_t base_lba = exfat_cluster_lba(target.cluster);
    uint8_t sector[512];

    if (exfat_sector_read(base_lba + last_sector_index, sector) != 0) return -1;

    uint32_t tail_offset = (uint32_t)((target.size - 1) % 512);
    int64_t remove_count = 0;

    if (sector[tail_offset] == '\n') {
        remove_count++;
        if (tail_offset > 0 && sector[tail_offset - 1] != '\n') {
            tail_offset--;
        }
    }

    while (tail_offset >= 0 && sector[tail_offset] != '\n') {
        remove_count++;
        if (tail_offset == 0) break;
        tail_offset--;
    }

    uint64_t new_len = (target.size > (uint64_t)remove_count) ? target.size - (uint64_t)remove_count : 0;

    if (exfat_sector_read(target.entry_lba, sector) != 0) return -1;

    exfat_dentry_file_t *file = (exfat_dentry_file_t *)&sector[target.entry_offset];
    exfat_dentry_stream_t *stream = (exfat_dentry_stream_t *)&sector[target.entry_offset + 32];

    stream->data_length = new_len;
    stream->valid_data_length = new_len;
    file->set_checksum = exfat_calc_entry_crc(&sector[target.entry_offset], file->secondary_count + 1);

    return exfat_sector_write(target.entry_lba, sector);
}

int exfat_print_directory(int hidden) {
    uint8_t sector[512];
    uint64_t base_lba = exfat_cluster_lba(g_current_cluster);

    dogeio_text_println("-------- In Current Folder --------");
    for (uint32_t s = 0; s < 8; s++) {
        if (exfat_sector_read(base_lba + s, sector) != 0) return -1;

        for (int i = 0; i < 512; i += 32) {
            if (sector[i] == 0x00) return 0;

            if (sector[i] == EXFAT_TYPE_FILE) {
                exfat_dentry_file_t *file = (exfat_dentry_file_t *)&sector[i];
                exfat_dentry_stream_t *stream = (exfat_dentry_stream_t *)&sector[i + 32];

                char name_buf[256] = {0};
                int pos = 0;

                for (uint8_t sec = 2; sec <= file->secondary_count; sec++) {
                    exfat_dentry_name_t *name_ent = (exfat_dentry_name_t *)&sector[i + sec * 32];
                    if (name_ent->entry_type == EXFAT_TYPE_NAME) {
                        for (int c = 0; c < 15; c++) {
                            uint16_t ch = name_ent->unicode_name[c];
                            if (ch == 0) break;
                            if (pos < 255) name_buf[pos++] = (char)ch;
                        }
                    }
                }

                if (!hidden && (name_buf[0] == '.' || (file->file_attributes & 0x02))) {
                    i += file->secondary_count * 32;
                    continue;
                }

                if (file->file_attributes & 0x10) {
                    dogeio_text_print("DIR  ");
                    dogeio_text_println(name_buf);
                } else {
                    char size_str[32];
                    exfat_num_to_str(stream->data_length, size_str);
                    char name_str[32];
                    str_pad(name_str, name_buf, 16, ' ');

                    dogeio_text_print("FILE ");
                    dogeio_text_print(name_str);
                    dogeio_text_print(" | ");
                    dogeio_text_print(size_str);
                    dogeio_text_println(" bytes");
                }

                i += file->secondary_count * 32;
            }
        }
    }
    return 0;
}

int exfat_change_directory(const char *path) {
    if (str_strcmp(path, "/") == 0 || str_strcmp(path, "..") == 0) {
        g_current_cluster = g_root_cluster;
        str_strcpy(g_current_path, "/");
        return 1;
    }

    exfat_target_t target;
    if (exfat_resolve_entry(path, &target) == 0 && target.is_dir) {
        g_current_cluster = target.cluster;
        str_strcpy(g_current_path, path);
        return 1;
    }
    return -1;
}

const char* exfat_get_working_dir(void) {
    return g_current_path;
}