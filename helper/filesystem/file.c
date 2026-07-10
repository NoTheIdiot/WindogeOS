#include <drivers.h>
#include <stdint.h>
#include <string.h>
#include <system.h>

uint32_t allocate_sectors_from_bitmap(uint8_t* bitmap, uint32_t sectors_needed) {
    uint32_t consecutive = 0;
    uint32_t start = 0;

    for (uint32_t s = 3; s < TOTAL_DISK_SECTORS; s++) {
        uint32_t bytes_idx = s / 8;
        uint32_t bit_idx = s % 8;

        if ((bitmap[bytes_idx] & (1 << bit_idx)) == 0) {
            if (consecutive == 0) start = s;
            consecutive++;

            if (consecutive == sectors_needed) {
                for (uint32_t i = start; i < start + sectors_needed; i++) {
                    bitmap[i / 8] |= (1 << (i % 8));
                }
                return start;
            }
        } else {
            consecutive = 0;
        }
    }
    return -1;  // too fragmented, idiot
}

void fs_format() {
    fs_superblock_t sb = {
        .magic = FS_MAGIC,
        .total_sectors = TOTAL_DISK_SECTORS,
        .file_count = 0
    };
    fs_disk_write(FS_SUPER_SECTOR, &sb);
    uint8_t empty_buffer[SECTOR_SIZE] = {0};
    fs_disk_write(FS_BITMAP_SECTOR, empty_buffer); 
    fs_disk_write(FS_DIR_SECTOR, empty_buffer);
}

int fs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    fs_superblock_t sb;
    uint8_t bitmap[SECTOR_SIZE];
    fs_inode_t dir_table[8];

    fs_disk_read(FS_SUPER_SECTOR, &sb);
    fs_disk_read(FS_BITMAP_SECTOR, bitmap);
    fs_disk_read(FS_DIR_SECTOR, dir_table);

    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (dir_table[i].is_used == 0) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;
    uint32_t sectors_needed = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint32_t sectors_allocated = 0;
    uint32_t extent_idx = 0;

    memset(&dir_table[slot], 0, sizeof(fs_inode_t));
    string_strncpy(dir_table[slot].filename, name, MAX_FILE_NAME);
    dir_table[slot].file_size = size;
    dir_table[slot].is_used = 1;
}