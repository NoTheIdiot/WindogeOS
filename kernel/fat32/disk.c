#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>

typedef struct disk_info
{
    uint32_t totalSectors;
    FILE* stream;   // actual disk data
    uint32_t byte_per_sector;
}disk_info_t;

disk_info_t Disk;

void writeSectors(const void* buffer, uint32_t lba, uint32_t sector_num)
{
    if(lba > Disk.totalSectors || (lba + sector_num) > Disk.totalSectors)
        return;

    fseek(Disk.stream, lba * Disk.byte_per_sector, SEEK_SET);

    fwrite(buffer, sizeof(uint8_t), Disk.byte_per_sector * sector_num, Disk.stream);
}

void readSectors(void* buffer, uint32_t lba, uint32_t sector_num)
{
    if(lba > Disk.totalSectors || (lba + sector_num) > Disk.totalSectors)
        return;

    fseek(Disk.stream, lba * Disk.byte_per_sector, SEEK_SET);

    fread(buffer, sizeof(uint8_t), Disk.byte_per_sector * sector_num, Disk.stream);
}

void disk_init(char* path)
{
    struct stat metainfo;

    Disk.byte_per_sector = 512;    // I'm assuming

    stat(path, &metainfo);

    Disk.totalSectors = metainfo.st_size / Disk.byte_per_sector;
    Disk.stream = fopen(path, "rb+");  // now our data stream

    if(Disk.stream == NULL)
    {
        perror("error while reading disk");
        exit(1);
    }
}