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

#define SUPERBLOCK_LBA   0
#define BITMAP_LBA       1
#define INODE_START_LBA  2
#define DATA_START_LBA   10 

#define INODES_PER_SECTOR ((uint32_t)(BLOCK_SIZE / sizeof(struct sfs_inode)))

struct sfs_superblock {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t inode_count;
};

struct sfs_inode {
    uint8_t  used;
    char     filename[MAX_FILENAME];
    uint32_t size;
    uint32_t direct_blocks[DIRECT_POINTERS]; 
};

typedef struct {
    char name[MAX_FILENAME];
    int id;
} fs_t;

extern fs_t fs_index[MAX_FILES];
extern int next_empty_file;

int sfs_format(void);
int sfs_create(char *name);
int sfs_write(const char *filename, const uint8_t *buffer, uint32_t count);
int sfs_read(int inode_idx, uint8_t *output_buffer, uint32_t max_bytes);
int sfs_list_directory(void);
int find_inode_by_name(const char *name, uint32_t *out_lba, uint32_t *out_offset, struct sfs_inode *out_inode);

#endif