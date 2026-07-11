#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#define MAX_LINES 1024
#define MAX_CHARS 256

// shell stuff
void system_dogeshell();
void system_dogeshell_execute(char* command);

// information stuff
void system_info_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void system_info_ram(char *out_buffer);
void system_info_cpu(char *out_vendor);

// file system
#define FS_MAGIC            0x45534653
#define SECTOR_SIZE         512
#define MAX_EXTENTS         4
#define MAX_FILE_NAME       31
#define TOTAL_DISK_SECTORS  65536

#define FS_SUPER_SECTOR     0
#define FS_BITMAP_SECTOR    1
#define FS_DIR_SECTOR       2

typedef struct {
    uint32_t start_sector;
    uint32_t sector_count;
} __attribute__((packed)) fs_extent_t;

typedef struct {
    char        filename[MAX_FILE_NAME + 1];
    uint32_t    file_size;
    uint8_t     is_used;
    uint8_t     extent_count;
    fs_extent_t extents[MAX_EXTENTS];
    uint8_t     padding[10];
} __attribute__((packed)) fs_inode_t;

typedef struct {
    uint32_t    magic;              
    uint32_t    total_sectors;      
    uint32_t    file_count;         
    uint8_t     padding[500];
} __attribute__((packed)) fs_superblock_t;

#define FS_DRIVE_MAGIC 0x446f6765

void fs_format();
void fs_init();
int fs_write_file(const char* name, const uint8_t* data, uint32_t size);
int fs_read_file(const char* filename, uint8_t* out_buffer);

#endif
