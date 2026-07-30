#include <dogeio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <basicutil.h>

#define ATA_DATA        0x1F0
#define ATA_FEATURES    0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7

#ifndef FS_BASE_LBA
#define FS_BASE_LBA     2048
#endif

extern void duolog(const char* message);

static void ata_Ready(void) {
    for (volatile int i = 0; i < 4; i++) {
        ports_inb(ATA_STATUS);
    }
    
    uint8_t status = ports_inb(ATA_STATUS);
    if (status == 0xFF) {
        return;
    }

    while ((ports_inb(ATA_STATUS) & 0x80) || !(ports_inb(ATA_STATUS) & 0x40));
}

static void ata_ReadSector(uint32_t lba, uint8_t *buffer) {
    ports_outb(ATA_DRIVE_HEAD, 0xE0 | (((lba + FS_BASE_LBA) >> 24) & 0x0F));
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)(lba + FS_BASE_LBA));
    ports_outb(ATA_LBA_MID,  (uint8_t)((lba + FS_BASE_LBA) >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)((lba + FS_BASE_LBA) >> 16));
    ports_outb(ATA_COMMAND,  0x20);

    ata_Ready();
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = ports_inw(ATA_DATA);
    }
}

static void ata_WriteSector(uint16_t lba, const uint8_t *buffer) {
    uint32_t target_lba = (uint32_t)lba + FS_BASE_LBA;
    
    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((target_lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)target_lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(target_lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(target_lba >> 16));
    ports_outb(ATA_COMMAND,  0x30); 

    ata_Ready();

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ports_outw(ATA_DATA, ptr[i]);
    }
    
    ports_outb(ATA_COMMAND, 0xE7); 
    for (volatile int i = 0; i < 1000; i++);
}

int fs_format(void) {
    uint8_t sector_buffer[BLOCK_SIZE];
    struct sfs_superblock *sb = (struct sfs_superblock*)sector_buffer;
    sb->magic = SFS_MAGIC;
    sb->total_blocks = TOTAL_BLOCKS;
    sb->inode_count = MAX_FILES;
    ata_WriteSector((uint16_t)SUPERBLOCK_LBA, sector_buffer);

    memset(sector_buffer, 0, BLOCK_SIZE);
    ata_WriteSector((uint16_t)BITMAP_LBA, sector_buffer);

    int total_inode_sectors = MAX_FILES / 8;
    if (MAX_FILES % 8 != 0) {
        total_inode_sectors++;
    }

    memset(sector_buffer, 0, BLOCK_SIZE);
    for (int i = 0; i < total_inode_sectors; i++) {
        ata_WriteSector((uint16_t)(INODE_START_LBA + i), sector_buffer);
    }
    return 0;
}

int fs_create(const char *name) {
    uint8_t sector_buffer[BLOCK_SIZE];
    struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;
    int total_inode_sectors = MAX_FILES / 8;
    if (MAX_FILES % 8 != 0) {
        total_inode_sectors++;
    }

    for (int sec = 0; sec < total_inode_sectors; sec++) {
        ata_ReadSector((uint32_t)(INODE_START_LBA + sec), sector_buffer);

        for (int i = 0; i < 8; i++) {
            if (!inodes[i].used) {
                inodes[i].used = 1;
                inodes[i].size = 0;
                memset(inodes[i].direct_blocks, 0, sizeof(inodes[i].direct_blocks));
                string_strncpy(inodes[i].filename, name, MAX_FILENAME);

                ata_WriteSector((uint16_t)(INODE_START_LBA + sec), sector_buffer);
                return (sec * 8) + i; 
            }
        }
    }
    return -1; 
}

int fs_write(int inode_idx, const uint8_t *buffer, uint32_t count) {
    if (inode_idx < 0 || inode_idx >= MAX_FILES) {
        return -1;
    }

    uint32_t max_capacity = DIRECT_POINTERS * BLOCK_SIZE;
    if (count > max_capacity) {
        count = max_capacity;
    }

    uint8_t inode_sector[BLOCK_SIZE];
    uint32_t target_inode_lba = (uint32_t)(INODE_START_LBA + (inode_idx / 8));
    uint32_t inner_offset = (uint32_t)(inode_idx % 8);

    ata_ReadSector(target_inode_lba, inode_sector);
    struct sfs_inode *file_inode = &((struct sfs_inode *)inode_sector)[inner_offset];
    if (!file_inode->used) {
        return -1;
    }

    uint8_t bitmap[BLOCK_SIZE];
    ata_ReadSector((uint32_t)BITMAP_LBA, bitmap);

    uint32_t bytes_written = 0;
    uint32_t block_index = 0;
    uint8_t data_sector[BLOCK_SIZE];
    int bitmap_changed = 0;

    while (bytes_written < count && block_index < DIRECT_POINTERS) {
        int free_block = -1;
        for (int b = 0; b < TOTAL_BLOCKS; b++) {
            int byte_idx = b / 8;
            int bit_idx = b % 8;
            if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                free_block = b;
                bitmap[byte_idx] |= (uint8_t)(1 << bit_idx); 
                bitmap_changed = 1;
                break;
            }
        }

        if (free_block == -1) break; 

        file_inode->direct_blocks[block_index] = (uint32_t)free_block;
        memset(data_sector, 0, BLOCK_SIZE);

        uint32_t chunk = (count - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (count - bytes_written);
        for (uint32_t i = 0; i < chunk; i++) {
            data_sector[i] = buffer[bytes_written + i];
        }

        ata_WriteSector((uint16_t)(DATA_START_LBA + free_block), data_sector);
        bytes_written += chunk;
        block_index++;
    }

    file_inode->size = bytes_written;
    ata_WriteSector((uint16_t)target_inode_lba, inode_sector);
    
    if (bitmap_changed) {
        ata_WriteSector((uint16_t)BITMAP_LBA, bitmap);
    }

    return (int)bytes_written;
}

int fs_read(int inode_idx, uint8_t *output_buffer, uint32_t max_bytes) {
    if (inode_idx < 0 || inode_idx >= MAX_FILES) {
        return -1;
    }

    uint8_t inode_sector[BLOCK_SIZE];
    uint32_t target_inode_lba = (uint32_t)(INODE_START_LBA + (inode_idx / 8));
    uint32_t inner_offset = (uint32_t)(inode_idx % 8);

    ata_ReadSector(target_inode_lba, inode_sector);
    struct sfs_inode *file_inode = &((struct sfs_inode *)inode_sector)[inner_offset];
    if (!file_inode->used) {
        return -1;
    }

    uint32_t bytes_to_read = (file_inode->size < max_bytes) ? file_inode->size : max_bytes;
    uint32_t bytes_read = 0;
    uint32_t block_index = 0;
    uint8_t data_sector[BLOCK_SIZE];

    while (bytes_read < bytes_to_read && block_index < DIRECT_POINTERS) {
        uint32_t active_block = file_inode->direct_blocks[block_index];
        
        if (active_block == 0 || active_block >= TOTAL_BLOCKS) {
            break;
        }

        ata_ReadSector((uint32_t)(DATA_START_LBA + active_block), data_sector);

        uint32_t chunk = (bytes_to_read - bytes_read > BLOCK_SIZE) ? BLOCK_SIZE : (bytes_to_read - bytes_read);
        for (uint32_t i = 0; i < chunk; i++) {
            output_buffer[bytes_read + i] = data_sector[i];
        }

        bytes_read += chunk;
        block_index++;
    }
    return (int)bytes_read;
}

int fs_list_directory(void) {
    uint8_t sector_buffer[BLOCK_SIZE];
    struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;
    int files_found = 0;
    int total_inode_sectors = MAX_FILES / 8;
    if (MAX_FILES % 8 != 0) {
        total_inode_sectors++;
    }

    for (int sec = 0; sec < total_inode_sectors; sec++) {
        ata_ReadSector((uint32_t)(INODE_START_LBA + sec), sector_buffer);

        for (int i = 0; i < 8; i++) {
            if (inodes[i].used == 1 && inodes[i].size != 0xFFFFFFFF) {
                if (inodes[i].filename[0] != '\0') {
                    dogeio_text_print(inodes[i].filename);
                } else {
                    dogeio_text_print("empty");
                }
                
                dogeio_text_print(" | ");

                uint32_t size = inodes[i].size;
                if (size == 0) {
                    dogeio_text_print("0");
                } else {
                    char num_buf[16]; 
                    int idx = 14;
                    num_buf[15] = '\0';
                    
                    while (size > 0 && idx >= 0) {
                        num_buf[idx] = (char)((size % 10) + '0');
                        size /= 10;
                        idx--;
                    }
                    dogeio_text_print(&num_buf[idx + 1]);
                }

                dogeio_text_println(" bytes");
                files_found++;
            }
        }
    }
    return files_found;
}
