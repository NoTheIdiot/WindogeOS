#ifndef FILE_H
#define FILE_H

/*
This works as a USTAR mod to add padding to allow read and writes.
I did this so I do not have to deal with FAT which wasted tons of 
time.
*/

typedef struct {
    char        name[32];
    uint32_t    start_sector;
    uint32_t    max_sectors;
    uint32_t    current_size_bytes;
    uint8_t     used;
    uint8_t     blank;
} fs;
bool read_disk_sector(uint32_t lba, uint8_t* buffer);
bool write_disk_sector(uint32_t lba, const uint8_t* buffer);
bool fs_format_disk();
bool fs_read_file(char* filename, uint8_t* output);
bool fs_write_file(const char* filename, const uint8_t* input_data, uint32_t num_bytes);
bool fs_create_file(const char* filename, uint32_t allocate_sectors);
bool fs_delete_file(const char* filename);

#endif
