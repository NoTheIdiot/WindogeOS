// Full screen fast-streaming blit routine
int vbe_stream_wallpaper(const char* dir_path, const char* name, const char* extension) {
    if (!vbe_initialized || vbe_bits_per_pixel != 32) {
        return -1; // Fail if not locked into 32bpp linear graphics mode
    }

    // Locate the first cluster of the raw .img file
    uint32_t current_cluster = get_file_first_cluster(dir_path, name, extension);
    if (current_cluster == 0xFFFFFFFF) {
        return -1; // File entry not found
    }

    // Point directly to the start of the VBE linear memory pointer address
    uint8_t* vram_row_ptr = (uint8_t*)vbe_frame_buffer;
    int current_row = 0;

    // Stream exactly 768 clusters sequentially into your 768 screen pixel rows
    while (current_cluster < 0x0FFFFFF8 && current_row < vbe_height) {
        
        // Blast the cluster sector blocks straight onto the physical video memory address!
        // No temporary buffer middlemen, zero memory copy overhead, zero memory allocation
        read_cluster(current_cluster, vram_row_ptr);

        // Advance your pointer target by exactly your physical hardware row stride (vbe_pitch)
        vram_row_ptr += vbe_pitch;
        current_row++;

        // Fetch next link chunk inside your FAT link map chain table
        current_cluster = get_next_cluster(current_cluster);
    }

    return 0; // Display update completed successfully!
}

// Helper to look up a directory file record entry and extract its cluster ID
static uint32_t get_file_first_cluster(const char* dir_path, const char* name, const char* extension) {
    // We look up the directory cluster index matching your path parameter
    int32_t dir_cluster = find_dir_cluster_by_path(dir_path);
    if (dir_cluster == -1) return 0xFFFFFFFF;

    uint32_t current_cluster = (uint32_t)dir_cluster;
    uint8_t sector_buffer[512];

    // Traverse directory sector regions to find the 8.3 matching name token
    while (current_cluster < 0x0FFFFFF8) {
        uint32_t sector_start = cluster_to_sector(current_cluster);
        
        for (uint8_t s = 0; s < fs.sectors_per_cluster; s++) {
            ata_read_sector(sector_start + s, sector_buffer);
            
            // Loop through all 16 potential 32-byte records inside a standard 512-byte sector block
            for (int i = 0; i < 512; i += 32) {
                fat32_dir_t* entry = (fat32_dir_t*)&sector_buffer[i];
                
                // End of directory entries condition check
                if (entry->filename[0] == 0x00) return 0xFFFFFFFF;
                // Skipped/Deleted directory record condition check
                if (entry->filename[0] == 0xE5) continue;
                // Ignore Subdirectories or Volume Label indicators
                if (entry->attributes & (FAT32_ATTR_DIRECTORY | FAT32_ATTR_VOLUME_ID)) continue;

                // Match against your custom DOS string rules
                if (match_dos_name(entry->filename, name, 8) && match_dos_name(entry->ext, extension, 3)) {
                    // Reassemble and return the full 32-bit entry cluster address
                    return ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
                }
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return 0xFFFFFFFF;
}