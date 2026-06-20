#ifndef FILE_H
#define FILE_H

#include <stdint.h>

/* --- FAT32 File Attribute Bitmasks --- */
#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LONG_NAME (FAT32_ATTR_READ_ONLY | FAT32_ATTR_HIDDEN | FAT32_ATTR_SYSTEM | FAT32_ATTR_VOLUME_ID)

/* --- FAT32 Driver Application State Context --- */
typedef struct {
    uint32_t lba_offset;            /* Start LBA sector of active partition */
    uint32_t fat_start_sector;      /* Absolute sector position of FAT 1 */
    uint32_t data_start_sector;     /* Absolute starting cluster region sector */
    uint32_t root_cluster;          /* Number ID cluster referencing root index */
    uint8_t  sectors_per_cluster;   /* Total mapping data blocks per single chain group */
} fat32_context_t;

/* --- BIOS Parameter Block Structure (Standard 512-Byte Boot Sector layout) --- */
typedef struct __attribute__((packed)) {
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
    
    /* FAT32 Extended Section Extensions */
    uint32_t sectors_per_fat_32;
    uint16_t extended_flags;
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
} fat32_bpb_t;

/* --- 32-Byte Standard 8.3 Directory Entry Format Record Layout --- */
typedef struct __attribute__((packed)) {
    uint8_t  filename[8];
    uint8_t  ext[3];
    uint8_t  attributes;
    uint8_t  reserved_win_nt;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat32_dir_t;

/* --- Global Variable Handle Declarations --- */
extern fat32_context_t fs;
extern uint8_t global_cluster_buffer[512 * 8];

/* --- Driver Core Subsystem Pipeline Prototypes --- */

void ata_read_sector(uint32_t lba, uint8_t *target_buffer);
void ata_write_sector(uint32_t lba, uint8_t *source_buffer);

uint32_t cluster_to_sector(uint32_t cluster);
void read_cluster(uint32_t cluster, uint8_t *buffer);
void write_cluster(uint32_t cluster, uint8_t *buffer);

uint32_t get_next_cluster(uint32_t current_cluster);
void set_next_cluster(uint32_t current_cluster, uint32_t next_cluster);
uint32_t allocate_free_cluster(void);

int match_dos_name(const uint8_t* entry_name, const char* search_name, int len);
void format_83_name(const uint8_t* raw_name, const uint8_t* raw_ext, char* out_str);
void parse_to_83_format(const char* name, const char* extension, char* out_name, char* out_ext);

int32_t find_dir_cluster_by_path(const char* path);

/* --- High-Level User Space Engine Routing APIs --- */

int fat32_read_file(const char* dir_path, const char* name, const char* extension, uint8_t* buffer);
int fat32_create_file(const char* dir_path, const char* name, const char* extension);
int fat32_write_file(const char* dir_path, const char* name, const char* extension, uint8_t* buffer, uint32_t size);
int fat32_delete_file(const char* dir_path, const char* name, const char* extension);

void fat32_init(uint32_t partition_lba_start);
void fat32_list_root_directory(void);
int fat32_list_directory(const char* path);

#endif /* FILE_H */
