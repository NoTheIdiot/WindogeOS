#include "file.h"
#include "ports.h"

static fat32_context_t fs;

/* ========================================================================= */
/* 1. SECTOR DRIVE ENGINE (ATA PIO 28-bit Read Operation)                    */
/* ========================================================================= */

// Reads a single 512-byte sector directly into target memory using I/O ports
static void ata_read_sector(uint32_t lba, uint8_t *target_buffer) {
    // 1. Send the primary master drive selection along with the highest 4 bits of the LBA
    ports_outb(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 2. Specify that we are requesting a single data sector (1 block count)
    ports_outb(ATA_SECCOUNT, 1);
    
    // 3. Send out remaining parts of our 28-bit block target address
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // 4. Issue the formal PIO Sector Read Command (0x20)
    ports_outb(ATA_COMMAND, 0x20);

    // 5. Poll drive status register until the target is ready (Status bit 3: DRQ)
    while (!(ports_inb(ATA_STATUS) & 0x08)) {
        // Simple hardware wait loop (in a complete OS, use sleep/yield here)
    }

    // 6. Direct word data transfer: stream 256 words (512 bytes) using ports_insw/
    ports_insw(ATA_DATA, target_buffer, 256);
}

/* ========================================================================= */
/* 2. MATHEMATICAL LAYER (Cluster to Sector Conversions)                      */
/* ========================================================================= */

// Translates a relative cluster index directly into its absolute starting disk sector
static uint32_t cluster_to_sector(uint32_t cluster) {
    return ((cluster - 2) * fs.sectors_per_cluster) + fs.data_start_sector;
}

// Reads an entire cluster allocation block spanning multiple contiguous disk sectors
static void read_cluster(uint32_t cluster, uint8_t *buffer) {
    uint32_t target_sector = cluster_to_sector(cluster);
    for (uint32_t i = 0; i < fs.sectors_per_cluster; i++) {
        ata_read_sector(target_sector + i, buffer + (i * 512));
    }
}

/* ========================================================================= */
/* 3. HARDWARE ALLOCATION TABLES (FAT Linked List Engine)                    */
/* ========================================================================= */

// Looks inside the file allocation table to read the next cluster map link address
static uint32_t get_next_cluster(uint32_t current_cluster) {
    uint8_t sector_cache[512];
    
    // Each FAT entry is 4 bytes (32 bits)
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fs.fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    // Read the exact sector holding our entry into the temporary stack cache
    ata_read_sector(fat_sector, sector_cache);

    // Extract the raw 32-bit mapping value safely from the buffered data
    uint32_t raw_value = *(uint32_t*)&sector_cache[entry_offset];

    // FAT32 reserves the top 4 bits for internal tracking; mask to use only 28 bits
    return raw_value & 0x0FFFFFFF;
}

/* ========================================================================= */
/* 4. DRIVER LOGIC IMPLEMENTATION (Init & File Search API)                   */
/* ========================================================================= */

// Parses the primary master BPB boot block to discover operating layout boundaries
void fat32_init(uint32_t partition_lba_start) {
    uint8_t boot_sector[512];
    fs.lba_offset = partition_lba_start;
    
    // Read absolute block sector zero of the target disk partition
    ata_read_sector(fs.lba_offset, boot_sector);
    
    // Safely typecast our memory chunk to read the dynamic layout map properties
    fat32_bpb_t* bpb = (fat32_bpb_t*)boot_sector;

    // Store calculations inside driver runtime states
    fs.sectors_per_cluster = bpb->sectors_per_cluster;
    fs.root_cluster = bpb->root_cluster;
    
    fs.fat_start_sector = fs.lba_offset + bpb->reserved_sector_count;
    fs.data_start_sector = fs.fat_start_sector + (bpb->num_fats * bpb->sectors_per_fat_32);
}

// Compares simple DOS string fragments (handles standard spaces or trailing pads)
static int match_dos_name(const uint8_t* entry_name, const char* search_name, int len) {
    for (int i = 0; i < len; i++) {
        if (search_name[i] == '\0') {
            // Confirm the remainder of the filename contains empty space pads
            for (int j = i; j < len; j++) {
                if (entry_name[j] != ' ' && entry_name[j] != '\0') return 0;
            }
            return 1;
        }
        if (entry_name[i] != search_name[i]) return 0;
    }
    return 1;
}

// Traverses a single-level root cluster map to load a designated file payload
int fat32_read_file(const char* name, const char* extension, uint8_t* output_buffer) {
    // Dynamically allocate memory for our active cluster size loop
    // (512 bytes * Max standard sector layouts, safely fits inside OS stack space)
    uint8_t cluster_buffer[512 * 8]; 
    uint32_t current_cluster = fs.root_cluster;

    // Outer loop: Walk through the root directory's cluster chain
    while (current_cluster < 0x0FFFFFFF) {
        read_cluster(current_cluster, cluster_buffer);
        
        // Internal tracking variables
        uint32_t total_dir_entries = (fs.sectors_per_cluster * 512) / sizeof(fat32_dir_t);
        fat32_dir_t* dir = (fat32_dir_t*)cluster_buffer;

        // Inner loop: Scan every 32-byte directory entry structural offset inside the cluster
        for (uint32_t i = 0; i < total_dir_entries; i++) {
            // Null mark (0x00) signals absolute end of directory records
            if (dir[i].filename[0] == 0x00) return -1; 
            // 0xE5 flags a deleted file entry; skip to next index
            if (dir[i].filename[0] == 0xE5) continue;
            // Skip Long File Name (LFN) metadata attributes entirely 
            if (dir[i].attributes == FAT32_ATTR_LONG_NAME) continue;

            // Perform an 8.3 matching check against base filename and dot extension layouts
            if (match_dos_name(dir[i].filename, name, 8) && match_dos_name(dir[i].ext, extension, 3)) {
                // Reconstruct the start cluster pointer index from its split bit values
                uint32_t file_cluster = ((uint32_t)dir[i].first_cluster_high << 16) | dir[i].first_cluster_low;
                uint32_t bytes_left = dir[i].file_size;
                uint32_t output_offset = 0;

                // Stream the target file data until hitting the End-of-Chain marker
                while (file_cluster < 0x0FFFFFF8 && bytes_left > 0) {
                    uint32_t cluster_bytes = fs.sectors_per_cluster * 512;
                    uint32_t chunk_size = (bytes_left < cluster_bytes) ? bytes_left : cluster_bytes;

                    // Read current cluster content into temporary buffer stack
                    read_cluster(file_cluster, cluster_buffer);
                    
                    // Direct copy operation to extract data payload
                    for (uint32_t b = 0; b < chunk_size; b++) {
                        output_buffer[output_offset + b] = cluster_buffer[b];
                    }

                    output_offset += chunk_size;
                    bytes_left -= chunk_size;

                    // Traverse to the next chunk in the linked list file map
                    file_cluster = get_next_cluster(file_cluster);
                }
                return 0; // File read operation completed successfully
            }
        }
        
        // If not found in the current cluster, fetch the next directory cluster entry link
        current_cluster = get_next_cluster(current_cluster);
    }
    return -1; // File not found in path matching
}
