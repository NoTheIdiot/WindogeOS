#include <dogeio.h>
#include <system.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t jump[3];        // 0xEB, 0x76, and 0x90
    char fs_name[8];        // is "EXFAT   "
    uint8_t zeros[53];      // /dev/zero that shi
    uint64_t offset;        // offset
    uint64_t volume;        // volume length
    uint32_t foffset;       // fat offset, from FAT origin
    uint32_t flength;       // fat length, size of a FAT in unit of 
                            // sector
    uint32_t choffset;      // cluster heap offset origin from the top 
                            // of the volume.
    uint32_t ccount;        // cluster count, about of clusters on drive.
    uint32_t fcord;         // first cluster at root
    uint32_t serial;        // volume serial number
    uint16_t fs_revision;   // file system revision
    uint16_t volume_flags;  // bit 0 is active FAT, bit 1 is volume dirty,
                            // bit 2 is failure, rest are zeros
    uint8_t sector_shift;   // shifting up
    uint8_t sectors_pch;    // sectirs per cluster shift
    uint8_t fat_num;        // number of fat
    uint8_t drive;          // yo what drive are we
    uint8_t percent_used;   // percnt of drive used, 0xFF is invalid (-1)
    uint8_t reserved[7];    // guess what, more zeros.
    uint8_t code[390];      // thats alot of empty space
    uint16_t sig;           // signature
} exfat_start_sector;

typedef struct __attribute__((packed)) {
    
} file_entry;