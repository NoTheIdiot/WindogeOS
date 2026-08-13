#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

void menubar_draw();

// file system items
#define SFS_MAGIC        0x53465321  
#define BLOCK_SIZE       512
#define MAX_FILENAME     32
#define MAX_FILES        64
#define TOTAL_BLOCKS     4096        
#define DIRECT_POINTERS  12          

#define POINTERS_PER_BLOCK (BLOCK_SIZE / sizeof(uint64_t))

#define SUPERBLOCK_LBA   0
#define BITMAP_LBA       1
#define INODE_START_LBA  2

#define INODES_PER_SECTOR ((uint32_t)(BLOCK_SIZE / sizeof(struct sfs_inode)))
#define DATA_START_LBA   (INODE_START_LBA + ((MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR))

struct sfs_superblock {
    uint64_t magic;
    uint64_t total_blocks;
    uint64_t inode_count;
};

struct sfs_inode {
    uint8_t  used;
    char     filename[MAX_FILENAME];
    uint64_t size;
    uint64_t direct_blocks[DIRECT_POINTERS]; 
    uint64_t indirect_block;
};

typedef struct {
    char name[MAX_FILENAME];
    int id;
} fs_t;

extern fs_t fs_index[MAX_FILES];
extern int next_empty_file;

int sfs_format(void);
int sfs_create(char *name);
int sfs_delete(const char *filename);
int sfs_delete_last_line(const char *filename);
int sfs_list_directory(int show_hidden);
int sfs_write(const char *filename, const uint8_t *buffer, uint64_t count);
int sfs_append(const char *filename, const uint8_t *buffer, uint64_t count);
int sfs_read(int inode_idx, uint8_t *output_buffer, uint64_t max_bytes);
int find_inode_by_name(const char *name, uint64_t *out_lba, uint64_t *out_offset, struct sfs_inode *out_inode);

#endif