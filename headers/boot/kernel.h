#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

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

#define SFS_MAGIC        0x53465321  
#define EXTENT_MAGIC     0xF30A
#define MAX_FILENAME     37
#define MAX_FILES        64
#define TOTAL_BLOCKS     512      
#define INODE_EXTENTS    4
#define DIRECT_POINTERS  INODE_EXTENTS

#define SUPERBLOCK_LBA   0
#define BITMAP_LBA       1
#define INODE_START_LBA  2

#define INODES_PER_SECTOR ((uint32_t)(BLOCK_SIZE / sizeof(struct sfs_inode)))
#define DATA_START_LBA   (INODE_START_LBA + ((MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR))

extern int current_directory_id;

int sfs_format(void);
int sfs_create(char* name, int is_directory);
int sfs_write(const char *filename, const uint8_t *buffer, uint64_t count);
int sfs_append(const char *filename, const uint8_t *buffer, uint64_t count);
int sfs_read(int inode_idx, uint8_t *output_buffer, uint64_t max_bytes);
int sfs_delete(const char *filename);
int sfs_delete_last_line(const char *filename);
int sfs_list_directory(int show_hidden);
int sfs_chdir(char* foldername);
char* sfs_get_current_directory_name(void);

// image rendering stuff
void put_pixel(uint64_t x, uint64_t y, uint32_t color);
uint32_t rgb_to_u32(uint8_t red, uint8_t green, uint8_t blue);

#endif