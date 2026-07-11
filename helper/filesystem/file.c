#include <drivers.h>
#include <stdint.h>
#include <string.h>
#include <system.h>
#include <dogeio.h>

static uint8_t fs_scratch_buffer[SECTOR_SIZE] __attribute__((aligned(16)));

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
    return (uint32_t)-1;  // too fragmented, idiot
}

void fs_format() {
    fs_superblock_t sb = {
        .magic = FS_MAGIC,
        .total_sectors = TOTAL_DISK_SECTORS,
        .file_count = 0
    };
    
    memset(fs_scratch_buffer, 0, SECTOR_SIZE);
    memcpy(fs_scratch_buffer, &sb, sizeof(fs_superblock_t));
    fs_disk_write(FS_SUPER_SECTOR, fs_scratch_buffer);
    
    memset(fs_scratch_buffer, 0, SECTOR_SIZE);
    fs_disk_write(FS_BITMAP_SECTOR, fs_scratch_buffer); 
    
    uint32_t dir_sectors_needed = (sizeof(fs_inode_t) * 8 + SECTOR_SIZE - 1) / SECTOR_SIZE;
    for (uint32_t i = 0; i < dir_sectors_needed; i++) {
        fs_disk_write(FS_DIR_SECTOR + i, fs_scratch_buffer);
    }
}

void fs_init() {
    fs_superblock_t sb;
    
    fs_disk_read(FS_SUPER_SECTOR, fs_scratch_buffer);
    memcpy(&sb, fs_scratch_buffer, sizeof(fs_superblock_t));

    if (sb.magic == FS_DRIVE_MAGIC) {
        dogeio_text_print("FS: Valid footprint detected. Preserving system files.\n");
    } else {
        dogeio_text_print("FS: Unformatted volume identified. Running first-boot creation...\n");
        fs_format();
    }
}

int fs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    fs_superblock_t sb;
    uint8_t bitmap[SECTOR_SIZE];
    fs_inode_t dir_table[8];

    fs_disk_read(FS_SUPER_SECTOR, fs_scratch_buffer);
    memcpy(&sb, fs_scratch_buffer, sizeof(fs_superblock_t));
    
    fs_disk_read(FS_BITMAP_SECTOR, fs_scratch_buffer);
    memcpy(bitmap, fs_scratch_buffer, SECTOR_SIZE);
    
    uint32_t dir_sectors = (sizeof(fs_inode_t) * 8 + SECTOR_SIZE - 1) / SECTOR_SIZE;
    for (uint32_t i = 0; i < dir_sectors; i++) {
        fs_disk_read(FS_DIR_SECTOR + i, fs_scratch_buffer);
        uint32_t offset = i * SECTOR_SIZE;
        uint32_t chunk_size = (sizeof(fs_inode_t) * 8 - offset > SECTOR_SIZE) ? SECTOR_SIZE : (sizeof(fs_inode_t) * 8 - offset);
        memcpy(((uint8_t*)dir_table) + offset, fs_scratch_buffer, chunk_size);
    }

    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (dir_table[i].is_used == 0) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;
    uint32_t sectors_needed = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    if (sectors_needed == 0) sectors_needed = 1;

    uint32_t sectors_allocated = 0;
    uint32_t extent_idx = 0;

    memset(&dir_table[slot], 0, sizeof(fs_inode_t));
    string_strncpy(dir_table[slot].filename, name, MAX_FILE_NAME);
    dir_table[slot].file_size = size;
    dir_table[slot].is_used = 1;

    while (sectors_allocated < sectors_needed) {
        if (extent_idx >= MAX_EXTENTS) return -2;

        uint32_t remaining = sectors_needed - sectors_allocated;
        uint32_t start_sec = allocate_sectors_from_bitmap(bitmap, remaining);

        if (start_sec == (uint32_t)-1) {
            uint32_t try_size = remaining - 1;
            uint32_t found_fragment = 0;

            while (try_size > 0) {
                start_sec = allocate_sectors_from_bitmap(bitmap, try_size);
                if (start_sec != (uint32_t)-1) {
                    dir_table[slot].extents[extent_idx].start_sector = start_sec;
                    dir_table[slot].extents[extent_idx].sector_count = try_size;
                    sectors_allocated += try_size;
                    found_fragment = 1;
                    break;
                }
                try_size--;
            }
            if (!found_fragment) return -3;
        } else {
            dir_table[slot].extents[extent_idx].start_sector = start_sec;
            dir_table[slot].extents[extent_idx].sector_count = remaining;
            sectors_allocated += remaining;
        }
        extent_idx++;
    }
    dir_table[slot].extent_count = extent_idx;
    uint32_t data_pointer = 0;
    for (uint32_t e = 0; e < dir_table[slot].extent_count; e++) {
        fs_extent_t ext = dir_table[slot].extents[e];
        for (uint32_t s = 0; s < ext.sector_count; s++) {
            memset(fs_scratch_buffer, 0, SECTOR_SIZE);
            
            uint32_t chunk = SECTOR_SIZE;
            if (data_pointer < size) {
                uint32_t bytes_left = size - data_pointer;
                if (bytes_left < SECTOR_SIZE) {
                    chunk = bytes_left;
                }
            } else {
                chunk = 0;
            }
            
            if (chunk > 0) {
                memcpy(fs_scratch_buffer, data + data_pointer, chunk);
                data_pointer += chunk;
            }
            
            fs_disk_write(ext.start_sector + s, fs_scratch_buffer);
        }
    }
    sb.file_count++;
    
    memset(fs_scratch_buffer, 0, SECTOR_SIZE);
    memcpy(fs_scratch_buffer, &sb, sizeof(fs_superblock_t));
    fs_disk_write(FS_SUPER_SECTOR, fs_scratch_buffer);
    
    memset(fs_scratch_buffer, 0, SECTOR_SIZE);
    memcpy(fs_scratch_buffer, bitmap, SECTOR_SIZE);
    fs_disk_write(FS_BITMAP_SECTOR, fs_scratch_buffer);

    for (uint32_t i = 0; i < dir_sectors; i++) {
        memset(fs_scratch_buffer, 0, SECTOR_SIZE);
        uint32_t offset = i * SECTOR_SIZE;
        uint32_t chunk_size = (sizeof(fs_inode_t) * 8 - offset > SECTOR_SIZE) ? SECTOR_SIZE : (sizeof(fs_inode_t) * 8 - offset);
        memcpy(fs_scratch_buffer, ((uint8_t*)dir_table) + offset, chunk_size);
        fs_disk_write(FS_DIR_SECTOR + i, fs_scratch_buffer);
    }

    return 0;
}

int fs_read_file(const char* filename, uint8_t* out_buffer) {
    fs_inode_t dir_table[8];
    
    uint32_t dir_sectors = (sizeof(fs_inode_t) * 8 + SECTOR_SIZE - 1) / SECTOR_SIZE;
    for (uint32_t i = 0; i < dir_sectors; i++) {
        fs_disk_read(FS_DIR_SECTOR + i, fs_scratch_buffer);
        uint32_t offset = i * SECTOR_SIZE;
        uint32_t chunk_size = (sizeof(fs_inode_t) * 8 - offset > SECTOR_SIZE) ? SECTOR_SIZE : (sizeof(fs_inode_t) * 8 - offset);
        memcpy(((uint8_t*)dir_table) + offset, fs_scratch_buffer, chunk_size);
    }

    // search for the filename
    fs_inode_t* target = 0;
    for (int i = 0; i < 8; i++) {
        if (dir_table[i].is_used && string_strcmp(dir_table[i].filename, filename) == 0) {
            target = &dir_table[i];
            break;
        }
    }
    if (!target) return -1;
    
    uint32_t bytes_read = 0;
    uint32_t file_size = target->file_size;

    for (uint32_t e = 0; e < target->extent_count; e++) {
        fs_extent_t ext = target->extents[e];
        for (uint32_t s = 0; s < ext.sector_count; s++) {
            fs_disk_read(ext.start_sector + s, fs_scratch_buffer);
            
            uint32_t chunk = SECTOR_SIZE;
            if (bytes_read < file_size) {
                uint32_t bytes_left = file_size - bytes_read;
                if (bytes_left < SECTOR_SIZE) {
                    chunk = bytes_left;
                }
            } else {
                chunk = 0;
            }

            if (chunk > 0) {
                memcpy(out_buffer + bytes_read, fs_scratch_buffer, chunk);
                bytes_read += chunk;
            }
        }
    }
    return (int)file_size;
}
