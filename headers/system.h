#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#define MAX_LINES 1024
#define MAX_CHARS 256

// shell stuff
void system_dogeshell();
void system_dogeshell_execute(char* command);

// information stuff
void system_info_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void system_info_ram(char *out_buffer);
void system_info_cpu(char *out_vendor);

// file system
#define FS_SECTOR_SIZE  512 
#define FS_READ_ONLY    0x01
#define FS_HIDDEN       0x02
#define FS_SYSTEM       0x04
#define FS_VOLUME_ID    0x08
#define FS_DIRECTORY    0x10
#define FS_ARCHIVE      0x20
#define FS_LONG_NAME    0x0F

struct __attribute__((packed)) mbr_partitionEntry {
    uint8_t boot_indicator;
    uint8_t starting_chs[3];
    uint8_t partition_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t total_sectors;
};

struct __attribute__((packed)) mbr_sector {
    uint8_t                     boot_code[446];
    struct mbr_partitionEntry   partitions[4];
    uint16_t                    boot_signature;
};

struct __attribute__((packed)) fs_vbr {
    uint8_t     jmp_code[3];
    char        oem_name[8];
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_clusters;
    uint16_t    reserved_sectors;
    uint8_t     num_fats;
    uint16_t    root_entries;
    uint16_t    total_sectors_16;
    uint8_t     media_descriptor;
    uint16_t    fat_size_16;
    uint16_t    sectors_per_track;
    uint16_t    sectors_per_track;
}

#endif
