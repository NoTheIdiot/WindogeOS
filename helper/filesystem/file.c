#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <bool.h>
#include <dogeio.h>
#include <lowlevel.h>

uint8_t lowlevel_ports_inb(uint16_t port);
uint16_t lowlevel_ports_inw(uint16_t port);
void lowlevel_ports_outb(uint16_t port, uint8_t value);

struct __attribute__((packed)) fat32_bpb {
    uint8_t boot_jump[3];
    char oem_id[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t dir_entries_count;
    uint16_t total_sectors_short;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat_short;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_large;
    uint32_t sectors_per_fat_large;
    uint16_t extended_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved_flags;
    uint8_t signature;
    uint32_t volume_id;
    char volume_label[11];
    char system_id[8];
};

struct __attribute__((packed)) fat32_dir_entry {
    char name[8];
    char ext[3];
    uint8_t attributes;
    uint8_t reserved_nt;
    uint8_t creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t cluster_low;
    uint32_t file_size;
};

static struct fat32_bpb global_bpb;
static uint32_t first_data_sector;

void system_file_read_sector(uint32_t lba, uint8_t *buffer) {
    uint32_t timeout = 0;
    while ((lowlevel_ports_inb(0x1F7) & 0x80)) {
        timeout++;
        if (timeout > 500000) {
            dogeio_text_println("not wow: filesystem has timeout, you cannot access files");
            dogeio_text_println("         much sad.");
            return; 
        }
    }

    lowlevel_ports_outb(0x1F6, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    lowlevel_ports_outb(0x1F2, 1);
    lowlevel_ports_outb(0x1F3, (uint8_t)lba);
    lowlevel_ports_outb(0x1F4, (uint8_t)(lba >> 8));
    lowlevel_ports_outb(0x1F5, (uint8_t)(lba >> 16));
    lowlevel_ports_outb(0x1F7, 0x20);

    while (!(lowlevel_ports_inb(0x1F7) & 0x08));

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = lowlevel_ports_inw(0x1F0);
    }
}

void system_file_init(void) {
    uint8_t sector0[512];
    system_file_read_sector(2048, sector0);
    
    struct fat32_bpb *bpb = (struct fat32_bpb *)sector0;
    global_bpb = *bpb;

    first_data_sector = 2048 + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat_large);
}

uint32_t system_file_cluster_to_sector(uint32_t cluster) {
    return ((cluster - 2) * global_bpb.sectors_per_cluster) + first_data_sector;
}

uint32_t system_file_get_next_cluster(uint32_t current_cluster) {
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = 2048 + global_bpb.reserved_sectors + (fat_offset / global_bpb.bytes_per_sector);
    uint32_t ent_offset = fat_offset % global_bpb.bytes_per_sector;

    uint8_t sector_data[512];
    system_file_read_sector(fat_sector, sector_data);

    uint32_t next_cluster = *(uint32_t *)(&sector_data[ent_offset]);
    return next_cluster & 0x0FFFFFFF;
}

void system_file_list_directory(void) {
    uint8_t dir_buf[512];
    uint32_t root_sector = system_file_cluster_to_sector(global_bpb.root_cluster);
    
    system_file_read_sector(root_sector, dir_buf);
    struct fat32_dir_entry *entry = (struct fat32_dir_entry *)dir_buf;

    for (int i = 0; i < 16; i++) {
        if (entry[i].name[0] == 0x00) {
            break;
        }
        if ((uint8_t)entry[i].name[0] == 0xE5 || entry[i].name[0] == '.') {
            continue;
        }
        if (entry[i].attributes & 0x0F) {
            continue;
        }

        char out_name[13];
        int out_idx = 0;

        for (int j = 0; j < 8; j++) {
            if (entry[i].name[j] != ' ') {
                out_name[out_idx++] = entry[i].name[j];
            }
        }

        if (entry[i].ext[0] != ' ') {
            out_name[out_idx++] = '.';
            for (int j = 0; j < 3; j++) {
                if (entry[i].ext[j] != ' ') {
                    out_name[out_idx++] = entry[i].ext[j];
                }
            }
        }
        out_name[out_idx] = '\0';

        dogeio_text_print("  ");
        dogeio_text_println(out_name);
    }
}

void system_file_output_file(const char *filename) {
    uint8_t dir_buf[512];
    uint32_t root_sector = system_file_cluster_to_sector(global_bpb.root_cluster);
    
    system_file_read_sector(root_sector, dir_buf);
    struct fat32_dir_entry *entry = (struct fat32_dir_entry *)dir_buf;

    uint32_t file_cluster = 0;
    bool found = false;

    for (int i = 0; i < 16; i++) {
        if (entry[i].name[0] == 0x00) break;
        if ((uint8_t)entry[i].name[0] == 0xE5 || (entry[i].attributes & 0x10)) continue;

        char fat_name[13];
        int idx = 0;
        for (int j = 0; j < 8; j++) {
            if (entry[i].name[j] != ' ') fat_name[idx++] = entry[i].name[j];
        }
        if (entry[i].ext[0] != ' ') {
            fat_name[idx++] = '.';
            for (int j = 0; j < 3; j++) {
                if (entry[i].ext[j] != ' ') fat_name[idx++] = entry[i].ext[j];
            }
        }
        fat_name[idx] = '\0';

        if (string_strcmp(fat_name, filename) == 0) {
            file_cluster = ((uint32_t)entry[i].cluster_high << 16) | entry[i].cluster_low;
            found = true;
            break;
        }
    }

    if (!found) {
        dogeio_text_print("File not found: ");
        dogeio_text_println((char *)filename);
        return;
    }

    uint32_t current_cluster = file_cluster;
    uint8_t cluster_buf[512]; 

    while (current_cluster < 0x0FFFFFF8 && current_cluster != 0) {
        uint32_t sector = system_file_cluster_to_sector(current_cluster);
        
        for (int s = 0; s < global_bpb.sectors_per_cluster; s++) {
            system_file_read_sector(sector + s, cluster_buf);
            
            for (int chunk = 0; chunk < 512; chunk++) {
                char c = (char)cluster_buf[chunk];
                if (c == '\0') goto file_done;
                
                char str_buf[2] = {c, '\0'};
                dogeio_text_print(str_buf);
            }
        }
        
        current_cluster = system_file_get_next_cluster(current_cluster);
    }

file_done:
    dogeio_text_print("\n");
}
