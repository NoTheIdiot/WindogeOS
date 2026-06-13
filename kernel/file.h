#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>

// Packed FAT32 Boot Record / BIOS Parameter Block Structure
typedef struct {
    uint8_t  bootjmp[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    // FAT32 Extended Fields
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

// Packed 32-byte Directory Entry Structure
typedef struct {
    uint8_t  filename[8];
    uint8_t  ext[3];
    uint8_t  attributes;
    uint8_t  reserved_win_nt;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_accessed_date;
    uint16_t first_cluster_high;
    uint16_t last_modified_time;
    uint16_t last_modified_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_t;

// Driver Global Context Tracking Structure
typedef struct {
    uint32_t lba_offset; // Starting LBA sector of the partition
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t sectors_per_cluster;
    uint32_t root_cluster;
} fat32_context_t;

// Public Driver API Constants and Declarations
#define FAT32_ATTR_SUBDIRECTORY 0x10
#define FAT32_ATTR_LONG_NAME    0x0F

void fat32_init(uint32_t partition_lba_start);
int fat32_read_file(const char* name, const char* extension, uint8_t* buffer);
int fat32_list_directory(const char* path);

#endif
