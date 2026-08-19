#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <bool.h>

void menubar_draw();

// file system items
#define ATA_DATA         0x1F0
#define ATA_FEATURES     0x1F1
#define ATA_SECTOR_CNT   0x1F2
#define ATA_LBA_LOW      0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HIGH     0x1F5
#define ATA_DRIVE_HEAD   0x1F6
#define ATA_COMMAND      0x1F7
#define ATA_STATUS       0x1F7

#ifndef FS_BASE_LBA
#define FS_BASE_LBA      4096
#endif

#ifndef BLOCK_SIZE
#define BLOCK_SIZE       512
#endif

int         exfat_wipe_and_format(void);
int         exfat_create_node(const char *name, bool is_dir);
int         exfat_write_file(const char *name, const uint8_t *data, uint64_t count);
int         exfat_append_file(const char *name, const uint8_t *data, uint64_t count);
int         exfat_read_file(const char *name, uint8_t *out_buf, uint64_t max_bytes);
int         exfat_delete_node(const char *name);
int         exfat_truncate_last_line(const char *name);
int         exfat_print_directory(void);
int         exfat_change_directory(const char *path);
const char* exfat_get_working_dir(void);

// image rendering stuff
void put_pixel(uint64_t x, uint64_t y, uint32_t color);
uint32_t rgb_to_u32(uint8_t red, uint8_t green, uint8_t blue);

#endif