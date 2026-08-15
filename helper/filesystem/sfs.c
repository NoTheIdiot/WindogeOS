/*
  a vfs so everything is easier later
  Updated with Single Indirect Block support
*/

#include <boot/kernel.h>
#include <dogeio.h>
#include <basicutil.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <bool.h>

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

#define POINTERS_PER_BLOCK (BLOCK_SIZE / sizeof(uint64_t))

extern void log(const char* message);

// 0 is root
int current_directory_id = 0;

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

static int ata_ReadSector(uint64_t lba, uint8_t *buffer) {
    uint64_t target_lba = lba + FS_BASE_LBA;
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

static int ata_WriteSector(uint64_t lba, const uint8_t *buffer) {
    uint64_t target_lba = lba + FS_BASE_LBA;
    
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

int find_inode_by_name(const char *name, uint64_t *out_lba, uint64_t *out_offset, struct sfs_inode *out_inode) {
    uint8_t sector_buffer[BLOCK_SIZE];
    uint64_t total_inode_sectors = (MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR;

    for (uint64_t sec = 0; sec < total_inode_sectors; sec++) {
        uint64_t current_lba = (uint64_t)(INODE_START_LBA + sec);
        if (ata_ReadSector(current_lba, sector_buffer) != 0) return -1;
        struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;

        for (uint64_t i = 0; i < INODES_PER_SECTOR; i++) {
            uint64_t idx = (sec * INODES_PER_SECTOR) + i;
            if (idx >= (uint64_t)MAX_FILES) break;

            if (inodes[i].used && str_strcmp(inodes[i].filename, (char*)name) == 0) {
                if (out_lba) *out_lba = current_lba;
                if (out_offset) *out_offset = i;
                if (out_inode) *out_inode = inodes[i];
                log("Fs: Inode Found");
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
    if (ata_WriteSector((uint64_t)SUPERBLOCK_LBA, sector_buffer) != 0) return -1;

    memset(sector_buffer, 0, BLOCK_SIZE);
    for (uint64_t b = 0; b < DATA_START_LBA; b++) {
        sector_buffer[b / 8] |= (uint8_t)(1U << (b % 8));
    }
    if (ata_WriteSector((uint64_t)BITMAP_LBA, sector_buffer) != 0) return -1;

    uint64_t total_inode_sectors = (MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR;
    for (uint64_t i = 0; i < total_inode_sectors; i++) {
        memset(sector_buffer, 0, BLOCK_SIZE);
        struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;
        
        for (uint64_t j = 0; j < INODES_PER_SECTOR; j++) {
            if ((i * INODES_PER_SECTOR) + j >= MAX_FILES) break;
            inodes[j].used = 0;
            inodes[j].size = 0;
            inodes[j].filename[0] = '\0';
            memset(inodes[j].direct_blocks, 0, sizeof(inodes[j].direct_blocks));
            inodes[j].indirect_block = 0;
        }

        if (ata_WriteSector((uint64_t)(INODE_START_LBA + i), sector_buffer) != 0) return -1;
    }
    log("Fs: disk formated");
    return 1;
}

int sfs_create(char* name, int is_directory) {
    if (!name || name[0] == '\0' || str_strlen(name) >= MAX_FILENAME) {
        log("fs (create): invalid filename");
        return -1;
    }

    if (find_inode_by_name(name, NULL, NULL, NULL) >= 0) {
        log("fs (create): file already exists");
        return -1;
    }

    uint8_t sector_buffer[BLOCK_SIZE];
    uint64_t total_inode_sectors = (MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR;

    for (uint64_t sec = 0; sec < total_inode_sectors; sec++) {
        uint64_t lba = (uint64_t)(INODE_START_LBA + sec);
        if (ata_ReadSector(lba, sector_buffer) != 0) {
            log("fs (create): failed to read inode sector");
            log("             you may want to try creating again.")
            return -1;
        }
        struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;

        for (uint64_t i = 0; i < INODES_PER_SECTOR; i++) {
            if ((sec * INODES_PER_SECTOR) + i >= (uint64_t)MAX_FILES) break;

            if (!inodes[i].used) {
                inodes[i].used = 1;
                inodes[i].size = 0;
                inodes[i].is_directory = is_directory ? 1 : 0;
                inodes[i].location = (uint8_t)current_directory_id;   // always set parent
                str_strcpy(inodes[i].filename, name);
                memset(inodes[i].direct_blocks, 0, sizeof(inodes[i].direct_blocks));
                inodes[i].indirect_block = 0;

                ata_WriteSector(lba, sector_buffer);

                if (is_directory == 0) log("Fs: file created");
                else log("Fs: directory created");

                return (int)((sec * INODES_PER_SECTOR) + i);
            }
        }
    }
    return -1;
}

static int find_free_block(uint8_t *bitmap) {
    for (uint64_t b = DATA_START_LBA; b < (uint64_t)TOTAL_BLOCKS; b++) {
        if (!(bitmap[b / 8] & (1U << (b % 8)))) {
            bitmap[b / 8] |= (uint8_t)(1U << (b % 8));
            return (int)b;
        }
    }
    return -1;
}

int sfs_write(const char *filename, const uint8_t *buffer, uint64_t count) {
    uint8_t sector_buffer[BLOCK_SIZE];
    uint8_t bitmap[BLOCK_SIZE];
    uint8_t data_sector[BLOCK_SIZE];
    uint8_t indirect_sector[BLOCK_SIZE];

    uint64_t target_lba, inner_offset;
    if (find_inode_by_name(filename, &target_lba, &inner_offset, NULL) < 0) {
        log("fs (write): file not found");
        return -2;
    }

    if (ata_ReadSector(target_lba, sector_buffer) != 0) {
        log("fs (write): failed to read inode sector");
        log("            you may want to try again.");
        return -1;
    }
    struct sfs_inode *file_inode = &((struct sfs_inode *)sector_buffer)[inner_offset];

    uint64_t max_capacity = (DIRECT_POINTERS + POINTERS_PER_BLOCK) * BLOCK_SIZE;
    if (count > max_capacity) count = max_capacity;

    if (ata_ReadSector((uint64_t)BITMAP_LBA, bitmap) != 0) {
        log("fs (write): failed to read bitmap");
        return -1;
    }

    // Free existing direct blocks
    for (int b = 0; b < DIRECT_POINTERS; b++) {
        uint64_t old_block = file_inode->direct_blocks[b];
        if (old_block > 0 && old_block < TOTAL_BLOCKS) {
            bitmap[old_block / 8] &= (uint8_t)~(1U << (old_block % 8));
            file_inode->direct_blocks[b] = 0;
        }
    }

    // Free existing indirect blocks and their contents
    if (file_inode->indirect_block > 0 && file_inode->indirect_block < TOTAL_BLOCKS) {
        if (ata_ReadSector(file_inode->indirect_block, indirect_sector) == 0) {
            uint64_t *pointers = (uint64_t *)indirect_sector;
            for (int i = 0; i < (int)POINTERS_PER_BLOCK; i++) {
                uint64_t block = pointers[i];
                if (block > 0 && block < TOTAL_BLOCKS) {
                    bitmap[block / 8] &= (uint8_t)~(1U << (block % 8));
                }
            }
        }
        bitmap[file_inode->indirect_block / 8] &= (uint8_t)~(1U << (file_inode->indirect_block % 8));
        file_inode->indirect_block = 0;
    }

    uint64_t bytes_written = 0;
    uint64_t block_index = 0;
    bool indirect_dirty = false;
    memset(indirect_sector, 0, BLOCK_SIZE);

    while (bytes_written < count && block_index < (DIRECT_POINTERS + POINTERS_PER_BLOCK)) {
        int free_block = find_free_block(bitmap);
        if (free_block == -1) {
            log("write err: disk full");
            break;
        }

        uint64_t new_block = (uint64_t)free_block;
        memset(data_sector, 0, BLOCK_SIZE);

        uint64_t chunk = (count - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (count - bytes_written);
        memcpy(data_sector, buffer + bytes_written, chunk);

        if (ata_WriteSector(new_block, data_sector) != 0) {
            log("write err: failed to write data block");
            return -1;
        }

        if (block_index < DIRECT_POINTERS) {
            file_inode->direct_blocks[block_index] = new_block;
        } else {
            if (file_inode->indirect_block == 0) {
                int free_ind = find_free_block(bitmap);
                if (free_ind == -1) {
                    log("write err: disk full (cannot allocate indirect block)");
                    bitmap[new_block / 8] &= ~(1U << (new_block % 8));
                    break;
                }
                file_inode->indirect_block = (uint64_t)free_ind;
            }
            uint64_t idx = block_index - DIRECT_POINTERS;
            uint64_t *pointers = (uint64_t *)indirect_sector;
            pointers[idx] = new_block;
            indirect_dirty = true;
        }

        bytes_written += chunk;
        block_index++;
    }

    if (file_inode->indirect_block != 0 && indirect_dirty) {
        ata_WriteSector(file_inode->indirect_block, indirect_sector);
    }

    file_inode->size = bytes_written;
    ata_WriteSector(target_lba, sector_buffer);
    ata_WriteSector((uint64_t)BITMAP_LBA, bitmap);

    log("write ok");
    return 1;
}

int sfs_append(const char *filename, const uint8_t *buffer, uint64_t count) {
    if (count == 0) return 0;

    uint8_t inode_sector[BLOCK_SIZE];
    uint8_t bitmap[BLOCK_SIZE];
    uint8_t data_sector[BLOCK_SIZE];
    uint8_t indirect_sector[BLOCK_SIZE];

    uint64_t target_lba, inner_offset;
    if (find_inode_by_name(filename, &target_lba, &inner_offset, NULL) < 0) {
        log("fs (append): file not found");
        return -1;
    }

    if (ata_ReadSector(target_lba, inode_sector) != 0) {
        log("append err: failed to read inode sector");
        return -1;
    }
    struct sfs_inode *file_inode = &((struct sfs_inode *)inode_sector)[inner_offset];

    uint64_t old_size = file_inode->size;
    uint64_t max_capacity = (DIRECT_POINTERS + POINTERS_PER_BLOCK) * BLOCK_SIZE;
    if (old_size >= max_capacity) {
        log("append err: file is full");
        return 0;
    }

    if (old_size + count > max_capacity) {
        count = max_capacity - old_size;
    }

    if (ata_ReadSector((uint64_t)BITMAP_LBA, bitmap) != 0) {
        log("append err: failed to read bitmap");
        return -1;
    }

    uint64_t bytes_written = 0;
    uint64_t current_block_idx = old_size / BLOCK_SIZE;
    uint64_t offset_in_block = old_size % BLOCK_SIZE;

    bool indirect_dirty = false;
    if (file_inode->indirect_block != 0) {
        ata_ReadSector(file_inode->indirect_block, indirect_sector);
    } else {
        memset(indirect_sector, 0, BLOCK_SIZE);
    }

    if (offset_in_block != 0 && bytes_written < count) {
        uint64_t active_block = 0;
        if (current_block_idx < DIRECT_POINTERS) {
            active_block = file_inode->direct_blocks[current_block_idx];
        } else {
            uint64_t idx = current_block_idx - DIRECT_POINTERS;
            uint64_t *pointers = (uint64_t *)indirect_sector;
            active_block = pointers[idx];
        }

        if (active_block != 0) {
            ata_ReadSector(active_block, data_sector);
            uint64_t chunk = BLOCK_SIZE - offset_in_block;
            if (chunk > count) chunk = count;
            memcpy(data_sector + offset_in_block, buffer + bytes_written, chunk);
            ata_WriteSector(active_block, data_sector);

            bytes_written += chunk;
            current_block_idx++;
        }
    }

    while (bytes_written < count && current_block_idx < (DIRECT_POINTERS + POINTERS_PER_BLOCK)) {
        int free_block = find_free_block(bitmap);
        if (free_block == -1) {
            log("append err: disk full");
            break;
        }

        uint64_t new_block = (uint64_t)free_block;
        memset(data_sector, 0, BLOCK_SIZE);

        uint64_t chunk = (count - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (count - bytes_written);
        memcpy(data_sector, buffer + bytes_written, chunk);
        ata_WriteSector(new_block, data_sector);

        if (current_block_idx < DIRECT_POINTERS) {
            file_inode->direct_blocks[current_block_idx] = new_block;
        } else {
            if (file_inode->indirect_block == 0) {
                int free_ind = find_free_block(bitmap);
                if (free_ind == -1) {
                    log("append err: disk full (cannot allocate indirect block)");
                    bitmap[new_block / 8] &= ~(1U << (new_block % 8));
                    break;
                }
                file_inode->indirect_block = (uint64_t)free_ind;
                memset(indirect_sector, 0, BLOCK_SIZE);
            }

            uint64_t idx = current_block_idx - DIRECT_POINTERS;
            if (idx >= POINTERS_PER_BLOCK) {
                bitmap[new_block / 8] &= ~(1U << (new_block % 8));
                break;
            }
            uint64_t *pointers = (uint64_t *)indirect_sector;
            pointers[idx] = new_block;
            indirect_dirty = true;
        }

        bytes_written += chunk;
        current_block_idx++;
    }

    if (file_inode->indirect_block != 0 && indirect_dirty) {
        ata_WriteSector(file_inode->indirect_block, indirect_sector);
    }

    if (bytes_written == 0) {
        return 0;
    }

    file_inode->size = old_size + bytes_written;
    ata_WriteSector(target_lba, inode_sector);
    ata_WriteSector((uint64_t)BITMAP_LBA, bitmap);

    log("append ok");
    return (int)bytes_written;
}

int sfs_delete(const char *filename) {
    uint8_t inode_sector[BLOCK_SIZE];
    uint8_t bitmap[BLOCK_SIZE];
    uint8_t indirect_sector[BLOCK_SIZE];

    uint64_t target_lba, inner_offset;
    if (find_inode_by_name(filename, &target_lba, &inner_offset, NULL) < 0) {
        log("delete err: file not found");
        return 0;
    }

    if (ata_ReadSector(target_lba, inode_sector) != 0) {
        log("delete err: failed to read inode sector");
        return -1;
    }

    struct sfs_inode *file_inode = &((struct sfs_inode *)inode_sector)[inner_offset];
    if (!file_inode->used) {
        log("delete err: file not used");
        return -1;
    }

    if (ata_ReadSector((uint64_t)BITMAP_LBA, bitmap) != 0) {
        log("delete err: failed to read bitmap");
        return -1;
    }

    for (int i = 0; i < DIRECT_POINTERS; i++) {
        uint64_t block = file_inode->direct_blocks[i];
        if (block > 0 && block < TOTAL_BLOCKS) {
            bitmap[block / 8] &= (uint8_t)~(1U << (block % 8));
            file_inode->direct_blocks[i] = 0;
        }
    }

    if (file_inode->indirect_block > 0 && file_inode->indirect_block < TOTAL_BLOCKS) {
        if (ata_ReadSector(file_inode->indirect_block, indirect_sector) == 0) {
            uint64_t *pointers = (uint64_t *)indirect_sector;
            for (int i = 0; i < (int)POINTERS_PER_BLOCK; i++) {
                uint64_t block = pointers[i];
                if (block > 0 && block < TOTAL_BLOCKS) {
                    bitmap[block / 8] &= (uint8_t)~(1U << (block % 8));
                }
            }
        }
        bitmap[file_inode->indirect_block / 8] &= (uint8_t)~(1U << (file_inode->indirect_block % 8));
        file_inode->indirect_block = 0;
    }

    file_inode->used = 0;
    file_inode->size = 0;
    file_inode->filename[0] = '\0';

    if (ata_WriteSector(target_lba, inode_sector) != 0) {
        log("delete err: failed to write inode sector");
        return -1;
    }
    if (ata_WriteSector((uint64_t)BITMAP_LBA, bitmap) != 0) {
        log("delete err: failed to write bitmap");
        return -1;
    }

    log("delete ok");
    return 1;
}

int sfs_delete_last_line(const char *filename) {
    uint64_t target_lba, inner_offset;
    if (find_inode_by_name(filename, &target_lba, &inner_offset, NULL) < 0) {
        log("delete last line err: file not found");
        return -1;
    }

    int inode_idx = find_inode_by_name(filename, NULL, NULL, NULL);
    if (inode_idx < 0) {
        log("delete last line err: file not found");
        return -1;
    }

    uint8_t buffer[(DIRECT_POINTERS + POINTERS_PER_BLOCK) * BLOCK_SIZE];
    int bytes_read = sfs_read(inode_idx, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        log("delete last line err: failed to read file");
        return -1;
    }

    if (bytes_read == 0) {
        return 0;
    }

    int end = bytes_read - 1;
    while (end >= 0 && buffer[end] == '\n') {
        end--;
    }

    int last_newline = -1;
    for (int i = end; i >= 0; i--) {
        if (buffer[i] == '\n') {
            last_newline = i;
            break;
        }
    }

    int new_size = (last_newline >= 0) ? (last_newline + 1) : 0;
    if (sfs_write(filename, buffer, (uint64_t)new_size) < 0) {
        log("delete last line err: failed to truncate file");
        return -1;
    }

    log("delete last line ok");
    return new_size;
}

int sfs_read(int inode_idx, uint8_t *output_buffer, uint64_t max_bytes) {
    if (inode_idx < 0 || (uint64_t)inode_idx >= MAX_FILES) return -1;

    uint8_t inode_sector[BLOCK_SIZE];
    uint64_t target_lba = (uint64_t)(INODE_START_LBA + ((uint64_t)inode_idx / INODES_PER_SECTOR));
    uint64_t offset = (uint64_t)((uint64_t)inode_idx % INODES_PER_SECTOR);

    if (ata_ReadSector(target_lba, inode_sector) != 0) {
        log("read err: failed to read inode sector");
        return -1;
    }
    struct sfs_inode *file_inode = &((struct sfs_inode *)inode_sector)[offset];
    if (!file_inode->used) return -1;

    uint64_t bytes_to_read = (file_inode->size < max_bytes) ? file_inode->size : max_bytes;
    uint64_t bytes_read = 0;
    uint8_t data_sector[BLOCK_SIZE];
    uint8_t indirect_sector[BLOCK_SIZE];
    bool indirect_loaded = false;

    uint64_t total_blocks_needed = (bytes_to_read + BLOCK_SIZE - 1) / BLOCK_SIZE;

    for (uint64_t block_index = 0; block_index < total_blocks_needed && bytes_read < bytes_to_read; block_index++) {
        uint64_t active_block = 0;

        if (block_index < DIRECT_POINTERS) {
            active_block = file_inode->direct_blocks[block_index];
        } else {
            if (file_inode->indirect_block == 0) break;
            
            if (!indirect_loaded) {
                if (ata_ReadSector(file_inode->indirect_block, indirect_sector) != 0) {
                    log("read err: failed to read indirect block");
                    return -1;
                }
                indirect_loaded = true;
            }
            
            uint64_t indirect_index = block_index - DIRECT_POINTERS;
            if (indirect_index >= POINTERS_PER_BLOCK) break;
            
            uint64_t *pointers = (uint64_t *)indirect_sector;
            active_block = pointers[indirect_index];
        }

        if (active_block == 0 || active_block >= TOTAL_BLOCKS) break;

        if (ata_ReadSector(active_block, data_sector) != 0) {
            log("read err: failed to read data block");
            return -1;
        }

        uint64_t chunk = (bytes_to_read - bytes_read > BLOCK_SIZE) ? BLOCK_SIZE : (bytes_to_read - bytes_read);
        memcpy(output_buffer + bytes_read, data_sector, chunk);
        bytes_read += chunk;
    }
    return (int)bytes_read;
}

int sfs_list_directory(int show_hidden) {
    uint8_t sector_buffer[BLOCK_SIZE];
    int files_found = 0;
    uint64_t total_inode_sectors = (MAX_FILES + INODES_PER_SECTOR - 1) / INODES_PER_SECTOR;

    dogeio_text_println("------ In Current Folder -----");

    if (current_directory_id != 0) {
        dogeio_text_print("DIR         ");
        uint32_t thing = dogeio_text_color;
        dogeio_text_color_change(0xADD8E6);
        char padded_name[17];
        str_pad(padded_name, "..", 16, ' ');
        dogeio_text_print(padded_name);
        dogeio_text_color_change(thing);
        dogeio_text_println(" | 0 bytes");
        files_found++;
    }

    for (uint64_t sec = 0; sec < total_inode_sectors; sec++) {
        if (ata_ReadSector((uint64_t)(INODE_START_LBA + sec), sector_buffer) != 0) continue;
        struct sfs_inode *inodes = (struct sfs_inode *)sector_buffer;

        for (uint64_t i = 0; i < INODES_PER_SECTOR; i++) {
            uint64_t idx = (sec * INODES_PER_SECTOR) + i;
            if (idx >= (uint64_t)MAX_FILES) break;

            if (inodes[i].used && inodes[i].location == current_directory_id) {
                if (inodes[i].filename[0] == '.' && !show_hidden) {
                    continue;
                }

                if (inodes[i].is_directory) {
                    dogeio_text_print("DIR         ");
                    uint32_t thing = dogeio_text_color;
                    dogeio_text_color_change(0xADD8E6);
                    char padded_name[17];
                    const char *name = (inodes[i].filename[0] != '\0') ? inodes[i].filename : "empty";
                    str_pad(padded_name, name, 16, ' ');
                    dogeio_text_print(padded_name);
                    dogeio_text_color_change(thing);
                } else {
                    dogeio_text_print("FILE        ");
                    char padded_name[17];
                    const char *name = (inodes[i].filename[0] != '\0') ? inodes[i].filename : "empty";
                    str_pad(padded_name, name, 16, ' ');
                    dogeio_text_print(padded_name);
                }

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

int sfs_chdir(char* foldername) {
    if (str_strcmp(foldername, "..") == 0) {
        uint8_t sector_buffer[BLOCK_SIZE];
        uint64_t target_lba = INODE_START_LBA + ((uint64_t)current_directory_id / INODES_PER_SECTOR);
        uint64_t offset     = (uint64_t)current_directory_id % INODES_PER_SECTOR;

        if (ata_ReadSector(target_lba, sector_buffer) == 0) {
            struct sfs_inode *inode = &((struct sfs_inode *)sector_buffer)[offset];
            current_directory_id = inode->location;  // go back to parent
            log("chdir ok (..)");
            return 1;
        }
        return -1;
    }

    // normal cd
    uint64_t target_lba, inner_offset;
    struct sfs_inode inode;
    int inode_idx = find_inode_by_name(foldername, &target_lba, &inner_offset, &inode);
    if (inode_idx < 0) {
        log("chdir err: folder not found");
        return -1;
    }
    if (!inode.is_directory) {
        log("chdir err: not a directory");
        return -2;
    }
    current_directory_id = inode_idx;
    log("chdir ok");
    return 1;
}


char* sfs_get_current_directory_name(void) {
    static char name[MAX_FILENAME];
    uint8_t sector_buffer[BLOCK_SIZE];

    uint64_t target_lba = INODE_START_LBA + ((uint64_t)current_directory_id / INODES_PER_SECTOR);
    uint64_t offset     = (uint64_t)current_directory_id % INODES_PER_SECTOR;

    if (ata_ReadSector(target_lba, sector_buffer) != 0) {
        return "unknown";
    }

    struct sfs_inode *inode = &((struct sfs_inode *)sector_buffer)[offset];
    if (inode->used && inode->is_directory) {
        str_strcpy(name, inode->filename);
        return name;
    }

    return "root";
}
