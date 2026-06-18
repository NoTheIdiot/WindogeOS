#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct fat_dir_entry
{
    char filename[11];            // File name (8 characters) and File extension (3 characters)
    uint8_t attributes;          // File attributes (read-only, hidden, system, etc.)
    uint8_t reserved;            // Reserved (usually zero)
    uint8_t creationTimeTenth;   // Creation time in tenths of a second
    uint16_t creationTime;       // Creation time (HHMMSS format)
    uint16_t creationDate;       // Creation date (YYMMDD format)
    uint16_t lastAccessDate;     // Last access date
    uint16_t firstClusterHigh;   // High word of the first cluster number (FAT32 only)
    uint16_t writeTime;          // Last write time
    uint16_t writeDate;          // Last write date
    uint16_t firstClusterLow;    // Low word of the first cluster number
    uint32_t fileSize;           // File size in bytes
} __attribute__((packed)) fat_dir_entry_t;

typedef struct location
{
    uint32_t cluster; // where this entry is stored in its parent
    uint32_t offset;  // offset within the sector of its parent

    bool has_lfn_entries;
    uint32_t lfn_start_cluster;
    uint32_t lfn_offset;
    uint32_t lfn_entry_count;
}location_t;

typedef struct file
{
    uint32_t readPos;
    fat_dir_entry_t entry;
    location_t in_parent;
    bool isRoot;
}file_t;

typedef enum {FILE_TYPE, DIR_TYPE}file_type;

typedef struct dirEntry
{
    char name[257]; // max 256 zero terminated fat32 filename
    file_type type;
}dirEntry_t;

void fat32_init();
file_t* fat32_open(const char* path);
void fat32_close(file_t* this_file);
int fat32_read(file_t* this_file, void* buffer, size_t size);
int fat32_write(file_t* this_file, void* buffer, size_t size);
int fat32_readdir(file_t* this_dir, dirEntry_t* out);
bool fat32_make(const char* path, bool isDirectory);
int fat32_delete(const char* path);