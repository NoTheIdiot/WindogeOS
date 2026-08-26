#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <bool.h>

static inline uint32_t rgb_to_u32(uint8_t red, uint8_t green, uint8_t blue) {
    return ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;
}

#define KERNEL_MENUBAR_DRAW             100
#define KERNEL_EXFAT_WIPE_AND_FORMAT    101
#define KERNEL_EXFAT_CREATE_NODE        102
#define KERNEL_EXFAT_WRITE_FILE         103
#define KERNEL_EXFAT_APPEND_FILE        104
#define KERNEL_EXFAT_READ_FILE          105
#define KERNEL_EXFAT_DELETE_NODE        106
#define KERNEL_EXFAT_TRUNCATE_LAST_LINE 107
#define KERNEL_EXFAT_PRINT_DIRECTORY    108
#define KERNEL_EXFAT_CHANGE_DIRECTORY   109
#define KERNEL_EXFAT_GET_WORKING_DIR    110
#define KERNEL_EXFAT_MOUNT              111
#define KERNEL_PUT_PIXEL                112

#ifdef RING3

#define KERNEL_MENUBAR_DRAW             100
#define KERNEL_EXFAT_WIPE_AND_FORMAT    101
#define KERNEL_EXFAT_CREATE_NODE        102
#define KERNEL_EXFAT_WRITE_FILE         103
#define KERNEL_EXFAT_APPEND_FILE        104
#define KERNEL_EXFAT_READ_FILE          105
#define KERNEL_EXFAT_DELETE_NODE        106
#define KERNEL_EXFAT_TRUNCATE_LAST_LINE 107
#define KERNEL_EXFAT_PRINT_DIRECTORY    108
#define KERNEL_EXFAT_CHANGE_DIRECTORY   109
#define KERNEL_EXFAT_GET_WORKING_DIR    110
#define KERNEL_EXFAT_MOUNT              111
#define KERNEL_PUT_PIXEL                112

#include <core.h>

static inline void menubar_draw(void) {
    core_syscall(KERNEL_MENUBAR_DRAW, 0, 0, 0, 0);
}

static inline int exfat_wipe_and_format(void) {
    return (int)core_syscall(KERNEL_EXFAT_WIPE_AND_FORMAT, 0, 0, 0, 0);
}

static inline int exfat_create_node(const char *name, bool is_dir) {
    return (int)core_syscall(KERNEL_EXFAT_CREATE_NODE, (uint64_t)name, (uint64_t)is_dir, 0, 0);
}

static inline int exfat_write_file(const char *name, const uint8_t *data, uint64_t count) {
    return (int)core_syscall(KERNEL_EXFAT_WRITE_FILE, (uint64_t)name, (uint64_t)data, count, 0);
}

static inline int exfat_append_file(const char *name, const uint8_t *data, uint64_t count) {
    return (int)core_syscall(KERNEL_EXFAT_APPEND_FILE, (uint64_t)name, (uint64_t)data, count, 0);
}

static inline int64_t exfat_read_file(const char *name, uint8_t *out_buf, uint64_t max_bytes) {
    return (int64_t)core_syscall(KERNEL_EXFAT_READ_FILE, (uint64_t)name, (uint64_t)out_buf, max_bytes, 0);
}

static inline int exfat_delete_node(const char *name) {
    return (int)core_syscall(KERNEL_EXFAT_DELETE_NODE, (uint64_t)name, 0, 0, 0);
}

static inline int exfat_truncate_last_line(const char *name) {
    return (int)core_syscall(KERNEL_EXFAT_TRUNCATE_LAST_LINE, (uint64_t)name, 0, 0, 0);
}

static inline int exfat_print_directory(int hidden) {
    return (int)core_syscall(KERNEL_EXFAT_PRINT_DIRECTORY, (uint64_t)hidden, 0, 0, 0);
}

static inline int exfat_change_directory(const char *path) {
    return (int)core_syscall(KERNEL_EXFAT_CHANGE_DIRECTORY, (uint64_t)path, 0, 0, 0);
}

static inline const char* exfat_get_working_dir(void) {
    return (const char*)core_syscall(KERNEL_EXFAT_GET_WORKING_DIR, 0, 0, 0, 0);
}

static inline int exfat_mount(void) {
    return (int)core_syscall(KERNEL_EXFAT_MOUNT, 0, 0, 0, 0);
}

static inline void put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    core_syscall(KERNEL_PUT_PIXEL, x, y, (uint64_t)color, 0);
}

#else

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

void        menubar_draw(void);
int         exfat_wipe_and_format(void);
int         exfat_create_node(const char *name, bool is_dir);
int         exfat_write_file(const char *name, const uint8_t *data, uint64_t count);
int         exfat_append_file(const char *name, const uint8_t *data, uint64_t count);
int64_t     exfat_read_file(const char *name, uint8_t *out_buf, uint64_t max_bytes);
int         exfat_delete_node(const char *name);
int         exfat_truncate_last_line(const char *name);
int         exfat_print_directory(int hidden);
int         exfat_change_directory(const char *path);
const char* exfat_get_working_dir(void);
int         exfat_mount(void);
void        put_pixel(uint64_t x, uint64_t y, uint32_t color);

#endif
#endif