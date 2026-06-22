#include "file.h"
#include "ports.h"

extern void dogeio_print(const char* str);
extern void dogeio_println(const char* str);

fat32_context_t fs;
uint8_t global_cluster_buffer[512 * 8];

/* --- Raw ATA Disk Sector Operations --- */

void ata_read_sector(uint32_t lba, uint8_t *target_buffer) {
    ports_outb(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ports_outb(ATA_SECCOUNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND, 0x20); // ATA READ SECTORS

    while (!(ports_inb(ATA_STATUS) & 0x08));
    ports_insw(ATA_DATA, target_buffer, 256);
}

void ata_write_sector(uint32_t lba, uint8_t *source_buffer) {
    ports_outb(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ports_outb(ATA_SECCOUNT, 1);
    ports_outb(ATA_LBA_LOW,  (uint8_t)lba);
    ports_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    ports_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    ports_outb(ATA_COMMAND, 0x30); // ATA WRITE SECTORS

    while (!(ports_inb(ATA_STATUS) & 0x08));
    ports_outsw(ATA_DATA, source_buffer, 256);
    
    while (ports_inb(ATA_STATUS) & 0x80); // Wait for BSY to clear
}

uint8_t fat32_read_byte_at_offset(uint32_t start_cluster, uint32_t offset) {
    uint32_t cluster_bytes = fs.sectors_per_cluster * 512;
    uint32_t current_cluster = start_cluster;

    while (offset >= cluster_bytes) {
        current_cluster = get_next_cluster(current_cluster);
        if (current_cluster >= 0x0FFFFFF8) return 0; 
        offset -= cluster_bytes;
    }

    read_cluster(current_cluster, global_cluster_buffer);
    return global_cluster_buffer[offset];
}

void fat32_write_byte_at_offset(uint32_t start_cluster, uint32_t offset, uint8_t data) {
    uint32_t cluster_bytes = fs.sectors_per_cluster * 512;
    uint32_t current_cluster = start_cluster;

    while (offset >= cluster_bytes) {
        current_cluster = get_next_cluster(current_cluster);
        if (current_cluster >= 0x0FFFFFF8) return; 
        offset -= cluster_bytes;
    }

    read_cluster(current_cluster, global_cluster_buffer);
    global_cluster_buffer[offset] = data;
    write_cluster(current_cluster, global_cluster_buffer);
}



uint32_t cluster_to_sector(uint32_t cluster) {
    return ((cluster - 2) * fs.sectors_per_cluster) + fs.data_start_sector;
}

void read_cluster(uint32_t cluster, uint8_t *buffer) {
    uint32_t target_sector = cluster_to_sector(cluster);
    for (uint32_t i = 0; i < fs.sectors_per_cluster; i++) {
        ata_read_sector(target_sector + i, buffer + (i * 512));
    }
}

void write_cluster(uint32_t cluster, uint8_t *buffer) {
    uint32_t target_sector = cluster_to_sector(cluster);
    for (uint32_t i = 0; i < fs.sectors_per_cluster; i++) {
        ata_write_sector(target_sector + i, buffer + (i * 512));
    }
}

/* --- File Allocation Table (FAT) Operations --- */

uint32_t get_next_cluster(uint32_t current_cluster) {
    uint8_t sector_cache[512];
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fs.fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    ata_read_sector(fat_sector, sector_cache);
    uint32_t raw_value = *(uint32_t*)&sector_cache[entry_offset];

    return raw_value & 0x0FFFFFFF;
}

void set_next_cluster(uint32_t current_cluster, uint32_t next_cluster) {
    uint8_t sector_cache[512];
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fs.fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    ata_read_sector(fat_sector, sector_cache);
    
    uint32_t *raw_ptr = (uint32_t*)&sector_cache[entry_offset];
    *raw_ptr = (*raw_ptr & 0xF0000000) | (next_cluster & 0x0FFFFFFF);

    ata_write_sector(fat_sector, sector_cache);
}

uint32_t allocate_free_cluster(void) {
    uint8_t sector_cache[512];
    uint32_t current_cluster = 2; 
    
    while (1) {
        uint32_t fat_offset = current_cluster * 4;
        uint32_t fat_sector = fs.fat_start_sector + (fat_offset / 512);
        uint32_t entry_offset = fat_offset % 512;

        ata_read_sector(fat_sector, sector_cache);
        
        while (entry_offset < 512) {
            uint32_t value = *(uint32_t*)&sector_cache[entry_offset] & 0x0FFFFFFF;
            if (value == 0x00000000) { 
                set_next_cluster(current_cluster, 0x0FFFFFFF); // Mark End Of Cluster Chain
                return current_cluster;
            }
            entry_offset += 4;
            current_cluster++;
        }
    }
    return 0; // Device is completely full
}

/* --- String Evaluation & Conversion Processing --- */

int match_dos_name(const uint8_t* entry_name, const char* search_name, int len) {
    for (int i = 0; i < len; i++) {
        if (search_name[i] == '\0') {
            for (int j = i; j < len; j++) {
                if (entry_name[j] != ' ' && entry_name[j] != '\0') return 0;
            }
            return 1;
        }
        if (entry_name[i] != search_name[i]) return 0;
    }
    return 1;
}

void format_83_name(const uint8_t* raw_name, const uint8_t* raw_ext, char* out_str) {
    int p = 0;
    for (int i = 0; i < 8; i++) {
        if (raw_name[i] != ' ' && raw_name[i] != '\0') {
            out_str[p++] = raw_name[i];
        }
    }
    if (raw_ext[0] != ' ' && raw_ext[0] != '\0') {
        out_str[p++] = '.';
        for (int i = 0; i < 3; i++) {
            if (raw_ext[i] != ' ' && raw_ext[i] != '\0') {
                out_str[p++] = raw_ext[i];
            }
        }
    }
    out_str[p] = '\0';
}

/* --- High-Level Directory & File Parsing Functions --- */

int32_t find_dir_cluster_by_path(const char* path) {
    uint32_t current_cluster = fs.root_cluster;
    if (!path || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        return current_cluster;
    }

    int path_idx = 0;
    if (path[0] == '/') path_idx++;

    char clean_name[11];

    while (path[path_idx] != '\0') {
        for (int i = 0; i < 11; i++) clean_name[i] = ' ';

        int name_p = 0;
        int ext_p = 8;
        int in_ext = 0;

        while (path[path_idx] != '/' && path[path_idx] != '\0') {
            if (path[path_idx] == '.') {
                in_ext = 1;
                path_idx++;
                continue;
            }
            if (!in_ext && name_p < 8) {
                clean_name[name_p++] = path[path_idx];
            } else if (in_ext && ext_p < 11) {
                clean_name[ext_p++] = path[path_idx];
            }
            path_idx++;
        }

        if (path[path_idx] == '/') path_idx++;

        int segment_found = 0;
        while (current_cluster < 0x0FFFFFF8) {
            read_cluster(current_cluster, global_cluster_buffer);

            uint32_t total_dir_entries = (fs.sectors_per_cluster * 512) / sizeof(fat32_dir_t);
            fat32_dir_t* dir = (fat32_dir_t*)global_cluster_buffer;

            for (uint32_t i = 0; i < total_dir_entries; i++) {
                if (dir[i].filename[0] == 0x00) break;
                if (dir[i].filename[0] == 0xE5) continue;
                if (dir[i].attributes == FAT32_ATTR_LONG_NAME) continue;

                if ((dir[i].attributes & 0x10) &&
                    match_dos_name(dir[i].filename, clean_name, 8) &&
                    match_dos_name(dir[i].ext, clean_name + 8, 3)) {

                    current_cluster = ((uint32_t)dir[i].first_cluster_high << 16) | dir[i].first_cluster_low;
                    if (current_cluster == 0) current_cluster = fs.root_cluster;
                    segment_found = 1;
                    break;
                }
            }
            if (segment_found) break;
            current_cluster = get_next_cluster(current_cluster);
        }
        if (!segment_found) return -1;
    }
    return current_cluster;
}

/* Helper to generate space-padded 8.3 arrays for internal directory checking */
void parse_to_83_format(const char* name, const char* extension, char* out_name, char* out_ext) {
    for (int i = 0; i < 8; i++) out_name[i] = ' ';
    int j = 0; while (name[j] != '\0' && j < 8) { out_name[j] = name[j]; j++; }

    for (int i = 0; i < 3; i++) out_ext[i] = ' ';
    j = 0; while (extension[j] != '\0' && j < 3) { out_ext[j] = extension[j]; j++; }
}

int fat32_read_file(const char* dir_path, const char* name, const char* extension, uint8_t* buffer) {
    int32_t target_dir = find_dir_cluster_by_path(dir_path);
    if (target_dir == -1) return -1;

    uint32_t current_cluster = (uint32_t)target_dir;
    char parsed_name[8], parsed_ext[3];
    parse_to_83_format(name, extension, parsed_name, parsed_ext);

    while (current_cluster < 0x0FFFFFF8) {
        read_cluster(current_cluster, global_cluster_buffer);
        uint32_t total_dir_entries = (fs.sectors_per_cluster * 512) / sizeof(fat32_dir_t);
        fat32_dir_t* dir = (fat32_dir_t*)global_cluster_buffer;

        for (uint32_t i = 0; i < total_dir_entries; i++) {
            if (dir[i].filename[0] == 0x00) return -1;
            if (dir[i].filename[0] == 0xE5) continue;
            if (dir[i].attributes == FAT32_ATTR_LONG_NAME) continue;
            if (dir[i].attributes & 0x10) continue;

            if (match_dos_name(dir[i].filename, (const char*)parsed_name, 8) &&
                match_dos_name(dir[i].ext, (const char*)parsed_ext, 3)) {

                uint32_t file_cluster = ((uint32_t)dir[i].first_cluster_high << 16) | dir[i].first_cluster_low;
                uint32_t bytes_to_read = dir[i].file_size;
                uint32_t total_read = 0;
                uint32_t cluster_bytes = fs.sectors_per_cluster * 512;
                uint8_t cluster_scratch[512 * 8];

                while (file_cluster < 0x0FFFFFF8 && total_read < bytes_to_read) {
                    read_cluster(file_cluster, cluster_scratch);
                    uint32_t left = bytes_to_read - total_read;
                    uint32_t chunk = (left < cluster_bytes) ? left : cluster_bytes;

                    for (uint32_t k = 0; k < chunk; k++) {
                        buffer[total_read + k] = cluster_scratch[k];
                    }
                    total_read += chunk;
                    file_cluster = get_next_cluster(file_cluster);
                }
                buffer[total_read] = '\0';
                return (int)total_read;
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return -1;
}

/* --- Writing, Modification, and Asset Creation Core Routines --- */

int fat32_create_file(const char* dir_path, const char* name, const char* extension) {
    int32_t target_dir = find_dir_cluster_by_path(dir_path);
    if (target_dir == -1) {
        return -1;
    }

    uint32_t current_cluster = (uint32_t)target_dir;
    char parsed_name[8], parsed_ext[3];
    parse_to_83_format(name, extension, parsed_name, parsed_ext);

    while (current_cluster < 0x0FFFFFF8) {
        read_cluster(current_cluster, global_cluster_buffer);
        uint32_t total_dir_entries = (fs.sectors_per_cluster * 512) / sizeof(fat32_dir_t);
        fat32_dir_t* dir = (fat32_dir_t*)global_cluster_buffer;

        for (uint32_t i = 0; i < total_dir_entries; i++) {
            if (dir[i].filename[0] == 0x00 || dir[i].filename[0] == 0xE5) {
                for (int k = 0; k < 8; k++) {
                    dir[i].filename[k] = parsed_name[k];
                }
                for (int k = 0; k < 3; k++) {
                    dir[i].ext[k] = parsed_ext[k];
                }

                dir[i].attributes = 0x00;
                dir[i].file_size = 0;

                uint32_t new_cluster = allocate_free_cluster();
                if (new_cluster == 0) {
                    return -2;
                }

                dir[i].first_cluster_low = (uint16_t)(new_cluster & 0xFFFF);
                dir[i].first_cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);

                write_cluster(current_cluster, global_cluster_buffer);
                return 0;
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return -1;
}

int fat32_write_file(const char* dir_path, const char* name, const char* extension, uint8_t* buffer, uint32_t size) {
    int32_t target_dir = find_dir_cluster_by_path(dir_path);
    if (target_dir == -1) return -1;

    uint32_t current_cluster = (uint32_t)target_dir;
    char parsed_name[8], parsed_ext[3];
    parse_to_83_format(name, extension, parsed_name, parsed_ext);

    while (current_cluster < 0x0FFFFFF8) {
        read_cluster(current_cluster, global_cluster_buffer);
        uint32_t total_dir_entries = (fs.sectors_per_cluster * 512) / sizeof(fat32_dir_t);
        fat32_dir_t* dir = (fat32_dir_t*)global_cluster_buffer;

        for (uint32_t i = 0; i < total_dir_entries; i++) {
            if (dir[i].filename[0] == 0x00) return -1;
            if (dir[i].filename[0] == 0xE5) continue;

            if (match_dos_name(dir[i].filename, (const char*)parsed_name, 8) &&
                match_dos_name(dir[i].ext, (const char*)parsed_ext, 3)) {

                uint32_t file_cluster = ((uint32_t)dir[i].first_cluster_high << 16) | dir[i].first_cluster_low;
                uint32_t old_size = dir[i].file_size;
                
                // calculate metadata
                dir[i].file_size = old_size + size; 
                write_cluster(current_cluster, global_cluster_buffer); // Save new size

                uint32_t cluster_bytes = fs.sectors_per_cluster * 512;
                
                uint32_t current_offset = old_size;
                while (current_offset >= cluster_bytes) {
                    uint32_t next = get_next_cluster(file_cluster);
                    if (next >= 0x0FFFFFF8) {
                        // Edge case: File size metadata was tracking, but no cluster allocated
                        next = allocate_free_cluster();
                        if (next == 0) return -2;
                        set_next_cluster(file_cluster, next);
                    }
                    file_cluster = next;
                    current_offset -= cluster_bytes;
                }

                uint32_t bytes_written = 0;
                uint8_t cluster_scratch[512 * 8]; // Ensure this matches your actual cluster size max

                while (bytes_written < size) {
                    // Always read the existing cluster data first to avoid destroying surrounding bytes!
                    read_cluster(file_cluster, cluster_scratch);

                    uint32_t cluster_space_left = cluster_bytes - current_offset;
                    uint32_t data_left_to_write = size - bytes_written;
                    uint32_t chunk = (data_left_to_write < cluster_space_left) ? data_left_to_write : cluster_space_left;

                    // Copy new data over the exact slice inside the scratch buffer
                    for (uint32_t k = 0; k < chunk; k++) {
                        cluster_scratch[current_offset + k] = buffer[bytes_written + k];
                    }

                    // Write the modified cluster back to storage
                    write_cluster(file_cluster, cluster_scratch);
                    
                    bytes_written += chunk;
                    current_offset = 0;

                    if (bytes_written < size) {
                        uint32_t next = get_next_cluster(file_cluster);
                        if (next >= 0x0FFFFFF8) {
                            next = allocate_free_cluster();
                            if (next == 0) return -2; // Out of disk space, go and get a job to get
                                                      // more storage.
                            set_next_cluster(file_cluster, next);
                        }
                        file_cluster = next;
                    }
                }
                return 0; // Success
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return -1;
}


int fat32_delete_file(const char* dir_path, const char* name, const char* extension) {
    int32_t target_dir = find_dir_cluster_by_path(dir_path);
    if (target_dir == -1) {
        return -1;
    }

    uint32_t current_cluster = (uint32_t)target_dir;
    char parsed_name[8], parsed_ext[3];
    parse_to_83_format(name, extension, parsed_name, parsed_ext);

    while (current_cluster < 0x0FFFFFF8) {
        read_cluster(current_cluster, global_cluster_buffer);
        uint32_t total_dir_entries = (fs.sectors_per_cluster * 512) / sizeof(fat32_dir_t);
        fat32_dir_t* dir = (fat32_dir_t*)global_cluster_buffer;

        for (uint32_t i = 0; i < total_dir_entries; i++) {
            if (dir[i].filename[0] == 0x00) {
                return -1;
            }
            if (dir[i].filename[0] == 0xE5) {
                continue;
            }

            if (match_dos_name(dir[i].filename, (const char*)parsed_name, 8) &&
                match_dos_name(dir[i].ext, (const char*)parsed_ext, 3)) {

                uint32_t file_cluster = ((uint32_t)dir[i].first_cluster_high << 16) | dir[i].first_cluster_low;

                while (file_cluster < 0x0FFFFFF8 && file_cluster != 0) {
                    uint32_t next = get_next_cluster(file_cluster);
                    set_next_cluster(file_cluster, 0x00000000);
                    file_cluster = next;
                }

                dir[i].filename[0] = 0xE5;
                write_cluster(current_cluster, global_cluster_buffer);
                return 0;
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return -1;
}

/* --- Mounting Initialization & Output Listing --- */

void fat32_init(uint32_t partition_lba_start) {
    uint8_t boot_sector[512];
    fs.lba_offset = partition_lba_start;

    ata_read_sector(fs.lba_offset, boot_sector);
    fat32_bpb_t* bpb = (fat32_bpb_t*)boot_sector;

    fs.sectors_per_cluster = bpb->sectors_per_cluster;
    fs.root_cluster = bpb->root_cluster;
    fs.fat_start_sector = fs.lba_offset + bpb->reserved_sector_count;
    fs.data_start_sector = fs.fat_start_sector + (bpb->num_fats * bpb->sectors_per_fat_32);
}

void fat32_list_root_directory(void) {
    fat32_list_directory("/");
}

int fat32_list_directory(const char* path) {
    int32_t path_cluster = find_dir_cluster_by_path(path);
    if (path_cluster == -1) {
        dogeio_print("Directory not found: ");
        dogeio_println(path);
        return -1;
    }

    uint32_t current_cluster = (uint32_t)path_cluster;
    char name_buffer[13];

    dogeio_println("Type    Name");
    dogeio_println("------------------------------------");

    while (current_cluster < 0x0FFFFFF8) {
        read_cluster(current_cluster, global_cluster_buffer);

        uint32_t total_dir_entries = (fs.sectors_per_cluster * 512) / sizeof(fat32_dir_t);
        fat32_dir_t* dir = (fat32_dir_t*)global_cluster_buffer;

        for (uint32_t i = 0; i < total_dir_entries; i++) {
            if (dir[i].filename[0] == 0x00) {
                return 0;
            }
            if (dir[i].filename[0] == 0xE5) {
                continue;
            }
            if (dir[i].attributes == FAT32_ATTR_LONG_NAME) {
                continue;
            }
            if (dir[i].attributes & 0x08) {
                continue;
            }

            format_83_name(dir[i].filename, dir[i].ext, name_buffer);

            if (dir[i].attributes & 0x10) {
                dogeio_print("FOLDER  ");
                dogeio_println(name_buffer);
            } else {
                dogeio_print("FILE    ");
                dogeio_println(name_buffer);
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return 0;
}

// there are a single edge case that i should mention
// windows (DOS/NT) uses \r\n
// POSIX uses \n
// what the f##k
int fat32_delete_line_by_number(const char* dir_path, const char* name, const char* extension, uint32_t line_number) {
    if (line_number == 0) return -1; // Line numbers usually start at 1

    int32_t target_dir = find_dir_cluster_by_path(dir_path);
    if (target_dir == -1) return -1;

    char parsed_name[8], parsed_ext[3];
    parse_to_83_format(name, extension, parsed_name, parsed_ext);

    uint32_t current_dir_cluster = (uint32_t)target_dir;
    
    while (current_dir_cluster < 0x0FFFFFF8) {
        read_cluster(current_dir_cluster, global_cluster_buffer);
        uint32_t total_dir_entries = (fs.sectors_per_cluster * 512) / sizeof(fat32_dir_t);
        fat32_dir_t* dir = (fat32_dir_t*)global_cluster_buffer;

        for (uint32_t i = 0; i < total_dir_entries; i++) {
            if (dir[i].filename[0] == 0x00) return -1;
            if (dir[i].filename[0] == 0xE5) continue;

            if (match_dos_name(dir[i].filename, (const char*)parsed_name, 8) &&
                match_dos_name(dir[i].ext, (const char*)parsed_ext, 3)) {

                uint32_t start_cluster = ((uint32_t)dir[i].first_cluster_high << 16) | dir[i].first_cluster_low;
                uint32_t original_size = dir[i].file_size;
                
                uint32_t line_start_offset = 0;
                uint32_t line_end_offset = 0;
                uint32_t current_line = 1;
                
                uint32_t cluster_bytes = fs.sectors_per_cluster * 512;
                uint8_t scratch[512 * 8]; // Match your cluster size max
                
                // --- STEP 1: SCAN FOR '\n' AND TRACK WINDOWS \r\n EDGE CASES ---
                uint32_t file_cluster = start_cluster;
                uint32_t global_offset = 0;
                int found_start = 0;
                int found_end = 0;

                while (file_cluster < 0x0FFFFFF8 && global_offset < original_size) {
                    read_cluster(file_cluster, scratch);
                    uint32_t bytes_to_read = (original_size - global_offset < cluster_bytes) ? 
                                             (original_size - global_offset) : cluster_bytes;
                    
                    for (uint32_t k = 0; k < bytes_to_read; k++) {
                        uint32_t absolute_byte_offset = global_offset + k;

                        // Catch the absolute beginning of the requested line
                        if (current_line == line_number && !found_start) {
                            line_start_offset = absolute_byte_offset;
                            found_start = 1;
                        }

                        // Trigger tracking when we hit a newline character
                        if (scratch[k] == '\n') {
                            if (current_line == line_number) {
                                line_end_offset = absolute_byte_offset + 1; // Deletion zone includes '\n'
                                found_end = 1;
                                break;
                            }
                            current_line++; // Increment line counter
                        }
                    }
                    if (found_end) break;
                    
                    global_offset += cluster_bytes;
                    file_cluster = get_next_cluster(file_cluster);
                }

                // If the line number requested is out of range, exit early
                if (!found_start) return -3; 

                // Last line of file does not have a trailing '\n'
                if (found_start && !found_end) {
                    line_end_offset = original_size;
                }

                uint32_t line_length = line_end_offset - line_start_offset;
                if (line_length == 0) return 0; // Empty line deletion, nothing to shift

                // --- STEP 2: STREAM SHIFT DATA FORWARD OVER THE DELETED ZONE ---
                uint32_t write_pointer = line_start_offset;
                uint32_t read_pointer = line_end_offset;

                while (read_pointer < original_size) {
                    // Use byte/block helpers to cascade data backwards without RAM caching
                    uint8_t byte_data = fat32_read_byte_at_offset(start_cluster, read_pointer);
                    fat32_write_byte_at_offset(start_cluster, write_pointer, byte_data);
                    
                    read_pointer++;
                    write_pointer++;
                }

                // --- STEP 3: UPDATE METADATA AND FREE REMAINING ORPHAN CLUSTERS ---
                uint32_t new_size = original_size - line_length;
                dir[i].file_size = new_size;
                write_cluster(current_dir_cluster, global_cluster_buffer); // Commit size change

                // Calculate exact number of clusters the shortened file needs
                uint32_t total_clusters_needed = (new_size + cluster_bytes - 1) / cluster_bytes;
                if (total_clusters_needed == 0) total_clusters_needed = 1; // Maintain base cluster allocation

                // Walk down the FAT chain to find our new End-of-Chain cluster
                file_cluster = start_cluster;
                for (uint32_t c = 1; c < total_clusters_needed; c++) {
                    file_cluster = get_next_cluster(file_cluster);
                }
                
                // Unlink trailing clusters and clear them inside the FAT
                uint32_t cluster_to_free = get_next_cluster(file_cluster);
                set_next_cluster(file_cluster, 0x0FFFFFFF); // Write standard EOC marker

                while (cluster_to_free < 0x0FFFFFF8 && cluster_to_free != 0) {
                    uint32_t next_free = get_next_cluster(cluster_to_free);
                    set_next_cluster(cluster_to_free, 0x00000000); // clear the fat index
                    cluster_to_free = next_free;
                }

                return 0; // Success
            }
        }
        current_dir_cluster = get_next_cluster(current_dir_cluster);
    }
    return -1; // File not found
}
