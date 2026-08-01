#include <dogeio.h>
#include <boot/kernel.h>
#include <stdint.h>
#include <string.h>
#include <bool.h>
#include <stddef.h>
#include <basicutil.h>

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
#define FS_BASE_LBA      8192
#endif

#ifndef BLOCK_SIZE
#define BLOCK_SIZE       512
#endif

extern void log(const char* message);

static int ata_Ready(void) {
    for (volatile int i = 0; i < 4; i++) {
        ports_inb(ATA_STATUS);
    }
    uint8_t status;
    do {
        status = ports_inb(ATA_STATUS);
        if (status & 0x01) return -1;
        if (status & 0x20) break;
    } while ((status & 0x80) || !(status & 0x08));
    return 0;
}

static int ata_ReadSector(uint32_t lba, uint8_t *buffer) {
    uint32_t target_lba = lba + FS_BASE_LBA;
    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((target_lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)target_lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(target_lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(target_lba >> 16));
    ports_outb(ATA_COMMAND,  0x20);

    if (ata_Ready() != 0) return -1;
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = ports_inw(ATA_DATA);
    }
    return 0;
}

static int ata_WriteSector(uint32_t lba, const uint8_t *buffer) {
    uint32_t target_lba = lba + FS_BASE_LBA;
    
    ports_outb(ATA_DRIVE_HEAD, 0xE0 | ((target_lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 4; i++) ports_inb(ATA_STATUS);

    ports_outb(ATA_SECTOR_CNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)target_lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(target_lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(target_lba >> 16));
    ports_outb(ATA_COMMAND,  0x30); 

    if (ata_Ready() != 0) return -1;

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ports_outw(ATA_DATA, ptr[i]);
    }
    
    ports_outb(ATA_COMMAND, 0xE7); 
    uint8_t status;
    do {
        status = ports_inb(ATA_STATUS);
        if (status & 0x01) return -1;
    } while (status & 0x80);
    return 0;
}

int find_inode_by_name(const char *name, uint32_t *out_lba, uint32_t *out_offset, struct sfs_inode *out_inode) {
    uint8_t sector_buffer[BLOCK_SIZE];
    uint32_t total_inode_sectors = (MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR;

    for (uint32_t sec = 0; sec < total_inode_sectors; sec++) {
        uint32_t current_lba = (uint32_t)(INODE_START_LBA + sec);
        if (ata_ReadSector(current_lba, sector_buffer) != 0) return -1;
        struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;

        for (uint32_t i = 0; i < INODES_PER_SECTOR; i++) {
            uint32_t idx = (sec * INODES_PER_SECTOR) + i;
            if (idx >= (uint32_t)MAX_FILES) break;

            if (inodes[i].used && str_strcmp(inodes[i].filename, (char*)name) == 0) {
                if (out_lba) *out_lba = current_lba;
                if (out_offset) *out_offset = i;
                if (out_inode) *out_inode = inodes[i];
                return (int)idx;
            }
        }
    }
    return -1;
}

int sfs_format(void) {
    uint8_t sector_buffer[BLOCK_SIZE];
    memset(sector_buffer, 0, BLOCK_SIZE);

    struct sfs_superblock *sb = (struct sfs_superblock*)sector_buffer;
    sb->magic = SFS_MAGIC;
    sb->total_blocks = TOTAL_BLOCKS;
    sb->inode_count = MAX_FILES;
    if (ata_WriteSector((uint32_t)SUPERBLOCK_LBA, sector_buffer) != 0) return -1;

    memset(sector_buffer, 0, BLOCK_SIZE);
    for (uint32_t b = 0; b < DATA_START_LBA; b++) {
        sector_buffer[b / 8] |= (uint8_t)(1U << (b % 8));
    }
    if (ata_WriteSector((uint32_t)BITMAP_LBA, sector_buffer) != 0) return -1;

    memset(sector_buffer, 0, BLOCK_SIZE);
    uint32_t total_inode_sectors = (MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR;
    for (uint32_t i = 0; i < total_inode_sectors; i++) {
        if (ata_WriteSector((uint32_t)(INODE_START_LBA + i), sector_buffer) != 0) return -1;
    }
    return 0;
}

int sfs_create(char* name) {
    if (!name || name[0] == '\0' || str_strlen(name) >= MAX_FILENAME) {
        log("create err: invalid filename\n");
        return -1;
    }

    if (find_inode_by_name(name, NULL, NULL, NULL) >= 0) {
        log("create err: file already exists\n");
        return -1;
    }

    uint8_t sector_buffer[BLOCK_SIZE];
    uint32_t total_inode_sectors = (MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR;

    for (uint32_t sec = 0; sec < total_inode_sectors; sec++) {
        uint32_t lba = (uint32_t)(INODE_START_LBA + sec);
        if (ata_ReadSector(lba, sector_buffer) != 0) {
            log("create err: failed to read inode sector\n");
            return -1;
        }
        struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;

        for (uint32_t i = 0; i < INODES_PER_SECTOR; i++) {
            if ((sec * INODES_PER_SECTOR) + i >= (uint32_t)MAX_FILES) break;
            
            if (!inodes[i].used) {
                inodes[i].used = 1;
                inodes[i].size = 0;
                memset(inodes[i].direct_blocks, 0, sizeof(inodes[i].direct_blocks));
                
                str_strncpy(inodes[i].filename, name, MAX_FILENAME - 1);
                inodes[i].filename[MAX_FILENAME - 1] = '\0';

                ata_WriteSector(lba, sector_buffer);
                return (int)((sec * INODES_PER_SECTOR) + i); 
            }
        }
    }
    return -1; 
}

static int find_free_block(uint8_t *bitmap) {
    for (uint32_t b = DATA_START_LBA; b < (uint32_t)TOTAL_BLOCKS; b++) {
        if (!(bitmap[b / 8] & (1U << (b % 8)))) {
            bitmap[b / 8] |= (uint8_t)(1U << (b % 8));
            return (int)b;
        }
    }
    return -1;
}

int sfs_write(const char *filename, const uint8_t *buffer, uint32_t count) {
    uint8_t sector_buffer[BLOCK_SIZE];
    uint8_t bitmap[BLOCK_SIZE];
    uint8_t data_sector[BLOCK_SIZE];

    uint32_t target_lba, inner_offset;
    if (find_inode_by_name(filename, &target_lba, &inner_offset, NULL) < 0) {
        log("write err: file not found\n");
        return -1;
    }

    if (ata_ReadSector(target_lba, sector_buffer) != 0) {
        log("write err: failed to read inode sector\n");
        return -1;
    }
    struct sfs_inode *file_inode = &((struct sfs_inode *)sector_buffer)[inner_offset];

    uint32_t max_capacity = DIRECT_POINTERS * BLOCK_SIZE;
    if (count > max_capacity) count = max_capacity;

    if (ata_ReadSector((uint32_t)BITMAP_LBA, bitmap) != 0) {
        log("write err: failed to read bitmap\n");
        return -1;
    }

    for (int b = 0; b < DIRECT_POINTERS; b++) {
        uint32_t old_block = file_inode->direct_blocks[b];
        if (old_block > 0 && old_block < TOTAL_BLOCKS) {
            bitmap[old_block / 8] &= (uint8_t)~(1U << (old_block % 8));
            file_inode->direct_blocks[b] = 0;
        }
    }

    uint32_t bytes_written = 0;
    uint32_t block_index = 0;

    while (bytes_written < count && block_index < DIRECT_POINTERS) {
        int free_block = find_free_block(bitmap);
        if (free_block == -1) {
            log("write err: disk full\n");
            break;
        }

        file_inode->direct_blocks[block_index] = (uint32_t)free_block;
        memset(data_sector, 0, BLOCK_SIZE);

        uint32_t chunk = (count - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (count - bytes_written);
        memcpy(data_sector, buffer + bytes_written, chunk);

        if (ata_WriteSector((uint32_t)free_block, data_sector) != 0) {
            log("write err: failed to write data block\n");
            return -1;
        }
        
        bytes_written += chunk;
        block_index++;
    }

    file_inode->size = bytes_written;
    ata_WriteSector(target_lba, sector_buffer);
    ata_WriteSector((uint32_t)BITMAP_LBA, bitmap);

    log("write ok\n");
    return (int)bytes_written;
}

int sfs_read(int inode_idx, uint8_t *output_buffer, uint32_t max_bytes) {
    if (inode_idx < 0 || (uint32_t)inode_idx >= MAX_FILES) return -1;

    uint8_t inode_sector[BLOCK_SIZE];
    uint32_t target_lba = (uint32_t)(INODE_START_LBA + ((uint32_t)inode_idx / INODES_PER_SECTOR));
    uint32_t offset = (uint32_t)((uint32_t)inode_idx % INODES_PER_SECTOR);

    if (ata_ReadSector(target_lba, inode_sector) != 0) {
        log("read err: failed to read inode sector\n");
        return -1;
    }
    struct sfs_inode *file_inode = &((struct sfs_inode *)inode_sector)[offset];
    if (!file_inode->used) return -1;

    uint32_t bytes_to_read = (file_inode->size < max_bytes) ? file_inode->size : max_bytes;
    uint32_t bytes_read = 0;
    uint8_t data_sector[BLOCK_SIZE];

    for (uint32_t block_index = 0; block_index < DIRECT_POINTERS && bytes_read < bytes_to_read; block_index++) {
        uint32_t active_block = file_inode->direct_blocks[block_index];
        if (active_block == 0 || active_block >= TOTAL_BLOCKS) break;

        if (ata_ReadSector(active_block, data_sector) != 0) {
            log("read err: failed to read data block\n");
            return -1;
        }

        uint32_t chunk = (bytes_to_read - bytes_read > BLOCK_SIZE) ? BLOCK_SIZE : (bytes_to_read - bytes_read);
        memcpy(output_buffer + bytes_read, data_sector, chunk);

        bytes_read += chunk;
    }
    return (int)bytes_read;
}

int sfs_list_directory(void) {
    uint8_t sector_buffer[BLOCK_SIZE];
    int files_found = 0;
    uint32_t total_inode_sectors = (MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR;

    for (uint32_t sec = 0; sec < total_inode_sectors; sec++) {
        if (ata_ReadSector((uint32_t)(INODE_START_LBA + sec), sector_buffer) != 0) continue;
        struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;

        for (uint32_t i = 0; i < INODES_PER_SECTOR; i++) {
            if ((sec * INODES_PER_SECTOR) + i >= (uint32_t)MAX_FILES) break;

            if (inodes[i].used) {
                dogeio_text_print(inodes[i].filename[0] != '\0' ? inodes[i].filename : "empty");
                dogeio_text_print(" | ");
                
                char num_buf[16];
                str_itoa((int)inodes[i].size, num_buf); 
                dogeio_text_print(num_buf);
                
                dogeio_text_println(" bytes");
                files_found++;
            }
        }
    }
    return files_found;
}