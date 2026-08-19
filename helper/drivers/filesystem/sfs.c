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

static const uint8_t exfat_upcase_default[128] = {
    0x61, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x41, 0x00
};

static inline int exfat_hw_ready(void) {
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);
    uint8_t status;
    do {
        status = ports_inb(ATA_STATUS);
        if (status & 0x21) return -1;
    } while ((status & 0x80) || !(status & 0x08));
    return 0;
}

static int exfat_sector_read(uint64_t lba, uint8_t *buffer) {
    if (lba > 0x0FFFFFFF) return -1;

    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND,  0x20);

    if (exfat_hw_ready() != 0) return -1;

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) ptr[i] = ports_inw(ATA_DATA);
    return 0;
}

static int exfat_sector_write(uint64_t lba, const uint8_t *buffer) {
    if (lba > 0x0FFFFFFF) return -1;

    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND,  0x30);

    if (exfat_hw_ready() != 0) return -1;

    const uint16_t *ptr = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) ports_outw(ATA_DATA, ptr[i]);

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

static inline uint64_t exfat_cluster_lba(uint32_t cluster) {
    return EXFAT_PARTITION_LBA + 2048 + ((uint64_t)(cluster - 2) * 8);
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

static uint32_t exfat_alloc_block(void) {
    uint8_t sector[512];
    uint64_t bitmap_lba = exfat_cluster_lba(2);

    if (exfat_sector_read(bitmap_lba, sector) != 0) return 0;

    for (int i = 0; i < 512; i++) {
        if (sector[i] != 0xFF) {
            for (int bit = 0; bit < 8; bit++) {
                if (!(sector[i] & (1 << bit))) {
                    sector[i] |= (1 << bit);
                    exfat_sector_write(bitmap_lba, sector);
                    return (i * 8) + bit + 2;
                }
            }
        }
    }
    return 0;
}

static void exfat_free_block(uint32_t cluster) {
    if (cluster < 2) return;
    uint8_t sector[512];
    uint64_t bitmap_lba = exfat_cluster_lba(2);

    if (exfat_sector_read(bitmap_lba, sector) != 0) return;

    uint32_t index = cluster - 2;
    uint32_t byte_pos = index / 8;
    uint8_t bit_pos = index % 8;

    if (byte_pos < 512) {
        sector[byte_pos] &= ~(1 << bit_pos);
        exfat_sector_write(bitmap_lba, sector);
    }
}

static int exfat_resolve_entry(const char *target_name, exfat_target_t *out) {
    uint8_t sector[512];
    uint64_t base_lba = exfat_cluster_lba(g_current_cluster);

    for (uint32_t s = 0; s < 8; s++) {
        if (exfat_sector_read(base_lba + s, sector) != 0) return -1;

        for (int i = 0; i < 512; i += 32) {
            if (sector[i] == 0x00) return -1;

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

                if (str_strcmp(name_buf, target_name) == 0) {
                    if (out) {
                        out->entry_lba = base_lba + s;
                        out->entry_offset = i;
                        out->cluster = stream->first_cluster;
                        out->size = stream->data_length;
                        out->is_dir = (file->file_attributes & 0x10) != 0;
                    }
                    return 0;
                }
                i += file->secondary_count * 32;
            }
        }
    }
    return -1;
}

static void exfat_num_to_str(uint32_t num, char *out) {
    if (num == 0) { out[0] = '0'; out[1] = '\0'; return; }
    char tmp[16]; int i = 0;
    while (num > 0) { tmp[i++] = '0' + (num % 10); num /= 10; }
    int j = 0;
    while (i > 0) { out[j++] = tmp[--i]; }
    out[j] = '\0';
}

int exfat_wipe_and_format(void) {
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
    exfat_sector_write(EXFAT_PARTITION_LBA + 0, sector);
    exfat_sector_write(EXFAT_PARTITION_LBA + 12, sector);

    uint8_t zero_sector[512] = {0};
    for (int i = 1; i <= 10; i++) {
        crc = exfat_calc_boot_crc(zero_sector, 512, crc, false);
        exfat_sector_write(EXFAT_PARTITION_LBA + i, zero_sector);
        exfat_sector_write(EXFAT_PARTITION_LBA + 12 + i, zero_sector);
    }

    uint32_t crc_table[128];
    for (int i = 0; i < 128; i++) crc_table[i] = crc;
    exfat_sector_write(EXFAT_PARTITION_LBA + 11, (uint8_t*)crc_table);
    exfat_sector_write(EXFAT_PARTITION_LBA + 23, (uint8_t*)crc_table);

    memset(sector, 0, 512);
    uint32_t *fat = (uint32_t *)sector;
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
    return 0;
}

int exfat_create_node(const char *name, bool is_dir) {
    uint8_t sector[512];
    uint64_t lba = exfat_cluster_lba(g_current_cluster);

    if (exfat_sector_read(lba, sector) != 0) return -1;

    int slot = -1;
    for (int i = 0; i <= 512 - 96; i += 32) {
        if ((sector[i] & 0x80) == 0 || sector[i] == 0x00) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    uint32_t new_cluster = exfat_alloc_block();
    if (new_cluster == 0) return -1;

    uint8_t len = (uint8_t)str_strlen(name);
    if (len > 15) len = 15;

    memset(&sector[slot], 0, 96);

    exfat_dentry_file_t *file = (exfat_dentry_file_t *)&sector[slot];
    file->entry_type = EXFAT_TYPE_FILE;
    file->secondary_count = 2;
    file->file_attributes = is_dir ? 0x10 : 0x20;

    exfat_dentry_stream_t *stream = (exfat_dentry_stream_t *)&sector[slot + 32];
    stream->entry_type = EXFAT_TYPE_STREAM;
    stream->flags = 0x03;
    stream->name_length = len;
    stream->first_cluster = new_cluster;

    exfat_dentry_name_t *fname = (exfat_dentry_name_t *)&sector[slot + 64];
    fname->entry_type = EXFAT_TYPE_NAME;
    for (uint8_t i = 0; i < len; i++) fname->unicode_name[i] = (uint16_t)name[i];

    stream->name_hash = exfat_calc_name_hash(fname->unicode_name, len);
    file->set_checksum = exfat_calc_entry_crc(&sector[slot], 3);

    return exfat_sector_write(lba, sector);
}

int exfat_write_file(const char *name, const uint8_t *data, uint64_t count) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0) return -1;

    uint8_t sector[512];
    uint64_t base_lba = exfat_cluster_lba(target.cluster);
    uint32_t sectors_needed = (count + 511) / 512;

    if (sectors_needed > 8) return -1;

    for (uint32_t i = 0; i < sectors_needed; i++) {
        memset(sector, 0, 512);
        uint32_t chunk = (count - (i * 512) > 512) ? 512 : (uint32_t)(count - (i * 512));
        memcpy(sector, data + (i * 512), chunk);
        if (exfat_sector_write(base_lba + i, sector) != 0) return -1;
    }

    if (exfat_sector_read(target.entry_lba, sector) != 0) return -1;
    exfat_dentry_file_t *file = (exfat_dentry_file_t *)&sector[target.entry_offset];
    exfat_dentry_stream_t *stream = (exfat_dentry_stream_t *)&sector[target.entry_offset + 32];

    stream->data_length = count;
    stream->valid_data_length = count;
    file->set_checksum = exfat_calc_entry_crc(&sector[target.entry_offset], file->secondary_count + 1);

    return exfat_sector_write(target.entry_lba, sector);
}

int exfat_append_file(const char *name, const uint8_t *data, uint64_t count) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0) return -1;
    if (target.size + count > 4096) return -1;

    uint8_t buffer[4096];
    uint64_t base_lba = exfat_cluster_lba(target.cluster);

    uint32_t existing = (target.size + 511) / 512;
    for (uint32_t i = 0; i < existing; i++) {
        exfat_sector_read(base_lba + i, buffer + (i * 512));
    }

    memcpy(buffer + target.size, data, count);
    return exfat_write_file(name, buffer, target.size + count);
}

int exfat_read_file(const char *name, uint8_t *out_buf, uint64_t max_bytes) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0) return -1;

    uint8_t sector[512];
    uint64_t base_lba = exfat_cluster_lba(target.cluster);
    uint64_t bytes_to_read = (target.size < max_bytes) ? target.size : max_bytes;

    uint8_t *dst = out_buf;
    uint64_t left = bytes_to_read;

    for (uint32_t s = 0; s < 8 && left > 0; s++) {
        if (exfat_sector_read(base_lba + s, sector) != 0) return -1;
        uint32_t chunk = (left > 512) ? 512 : (uint32_t)left;
        memcpy(dst, sector, chunk);
        dst += chunk;
        left -= chunk;
    }

    return (int)bytes_to_read;
}

int exfat_delete_node(const char *name) {
    exfat_target_t target;
    if (exfat_resolve_entry(name, &target) != 0) return -1;

    exfat_free_block(target.cluster);

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

    uint8_t buffer[4096];
    uint64_t base_lba = exfat_cluster_lba(target.cluster);

    uint32_t sectors = (target.size + 511) / 512;
    for (uint32_t i = 0; i < sectors; i++) {
        exfat_sector_read(base_lba + i, buffer + (i * 512));
    }

    int64_t new_len = target.size - 1;
    if (buffer[new_len] == '\n') new_len--;

    while (new_len >= 0 && buffer[new_len] != '\n') {
        new_len--;
    }

    new_len++;
    if (new_len < 0) new_len = 0;

    return exfat_write_file(name, buffer, (uint64_t)new_len);
}

int exfat_print_directory(void) {
    uint8_t sector[512];
    uint64_t base_lba = exfat_cluster_lba(g_current_cluster);

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

                if (file->file_attributes & 0x10) {
                    dogeio_text_print("[DIR]  ");
                    dogeio_text_println(name_buf);
                } else {
                    char size_str[16];
                    exfat_num_to_str((uint32_t)stream->data_length, size_str);

                    dogeio_text_print("[FILE] ");
                    dogeio_text_print(name_buf);
                    dogeio_text_print(" (");
                    dogeio_text_print(size_str);
                    dogeio_text_println(" bytes)");
                }

                i += file->secondary_count * 32;
            }
        }
    }
    return 0;
}

int exfat_change_directory(const char *path) {
    if (strcmp(path, "/") == 0 || strcmp(path, "..") == 0) {
        g_current_cluster = 4;
        str_strcpy(g_current_path, "/");
        return 0;
    }

    exfat_target_t target;
    if (exfat_resolve_entry(path, &target) == 0 && target.is_dir) {
        g_current_cluster = target.cluster;
        str_strcpy(g_current_path, path);
        return 0;
    }
    return -1;
}

const char* exfat_get_working_dir(void) {
    return g_current_path;
}