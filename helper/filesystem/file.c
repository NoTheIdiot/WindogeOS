#include <stdint.h>
#include "lowlevel.h"
#include "system.h"

#define STATUS_BSY  0x80
#define STATUS_DRQ  0x08
#define STATUS_DF   0x20
#define STATUS_ERR  0x01

static void ide_wait_ready(void) {
    while (lowlevel_ports_inb(0x1F7) & STATUS_BSY);
}

static void ide_wait_drq(void) {
    while (1) {
        uint8_t status = lowlevel_ports_inb(0x1F7);
        if (!(status & STATUS_BSY) && (status & STATUS_DRQ)) break;
    }
}

int disk_read(uint32_t lba, uint32_t sector_count, void* buffer) {
    uint8_t* ptr = (uint8_t*)buffer;

    for (uint32_t i = 0; i < sector_count; i++) {
        uint32_t current_lba = lba + i;

        ide_wait_ready();

        lowlevel_ports_outb(0x1F2, 1);
        lowlevel_ports_outb(0x1F3, (uint8_t)(current_lba & 0xFF));
        lowlevel_ports_outb(0x1F4, (uint8_t)((current_lba >> 8) & 0xFF));
        lowlevel_ports_outb(0x1F5, (uint8_t)((current_lba >> 16) & 0xFF));
        lowlevel_ports_outb(0x1F6, (uint8_t)(((current_lba >> 24) & 0x0F) | 0xE0));
        lowlevel_ports_outb(0x1F7, 0x20);

        ide_wait_drq();

        if (lowlevel_ports_inb(0x1F7) & (STATUS_DF | STATUS_ERR)) return -1;

        lowlevel_ports_insw(0x1F0, ptr, 256);
        ptr += 512;
    }
    return 0;
}

int disk_write(uint32_t lba, uint32_t sector_count, const void* buffer) {
    const uint8_t* ptr = (const uint8_t*)buffer;

    for (uint32_t i = 0; i < sector_count; i++) {
        uint32_t current_lba = lba + i;

        ide_wait_ready();

        lowlevel_ports_outb(0x1F2, 1);
        lowlevel_ports_outb(0x1F3, (uint8_t)(current_lba & 0xFF));
        lowlevel_ports_outb(0x1F4, (uint8_t)((current_lba >> 8) & 0xFF));
        lowlevel_ports_outb(0x1F5, (uint8_t)((current_lba >> 16) & 0xFF));
        lowlevel_ports_outb(0x1F6, (uint8_t)(((current_lba >> 24) & 0x0F) | 0xE0));
        lowlevel_ports_outb(0x1F7, 0x30);

        ide_wait_drq();

        if (lowlevel_ports_inb(0x1F7) & (STATUS_DF | STATUS_ERR)) return -1;

        lowlevel_ports_outsw(0x1F0, ptr, 256);
        ptr += 512;

        lowlevel_ports_outb(0x1F7, 0xE7);
        ide_wait_ready();
    }
    return 0;
}

static uint32_t get_next_cluster(struct FAT32_BPB* bpb, uint32_t current_cluster) {
    uint32_t fat_first_sector = bpb->hidden_sectors + bpb->reserved_sectors;
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fat_first_sector + (fat_offset / bpb->bytes_per_sector);
    uint32_t byte_offset = fat_offset % bpb->bytes_per_sector;

    uint8_t sector_buffer[512];
    if (disk_read(fat_sector, 1, sector_buffer) != 0) {
        return 0x0FFFFFF7;
    }

    return *(uint32_t*)&sector_buffer[byte_offset] & 0x0FFFFFFF;
}

static int set_next_cluster(struct FAT32_BPB* bpb, uint32_t current_cluster, uint32_t next_cluster) {
    uint32_t fat_first_sector = bpb->hidden_sectors + bpb->reserved_sectors;
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fat_first_sector + (fat_offset / bpb->bytes_per_sector);
    uint32_t byte_offset = fat_offset % bpb->bytes_per_sector;

    uint8_t sector_buffer[512];
    if (disk_read(fat_sector, 1, sector_buffer) != 0) return -1;

    uint32_t value = *(uint32_t*)&sector_buffer[byte_offset];
    value = (value & 0xF0000000) | (next_cluster & 0x0FFFFFFF);
    *(uint32_t*)&sector_buffer[byte_offset] = value;

    if (disk_write(fat_sector, 1, sector_buffer) != 0) return -1;

    for (uint8_t i = 1; i < bpb->fat_count; i++) {
        uint32_t mirror_sector = fat_sector + (i * bpb->sectors_per_fat_32);
        if (disk_write(mirror_sector, 1, sector_buffer) != 0) return -1;
    }

    return 0;
}

static uint32_t find_free_cluster(struct FAT32_BPB* bpb) {
    uint32_t fat_first_sector = bpb->hidden_sectors + bpb->reserved_sectors;
    uint8_t sector_buffer[512];
    uint32_t total_clusters = bpb->total_sectors_32 / bpb->sectors_per_cluster;

    for (uint32_t cluster = 2; cluster < total_clusters; cluster++) {
        uint32_t fat_offset = cluster * 4;
        uint32_t fat_sector = fat_first_sector + (fat_offset / bpb->bytes_per_sector);
        uint32_t byte_offset = fat_offset % bpb->bytes_per_sector;

        if (byte_offset == 0 || cluster == 2) {
            if (disk_read(fat_sector, 1, sector_buffer) != 0) return 0;
        }

        uint32_t value = *(uint32_t*)&sector_buffer[byte_offset] & 0x0FFFFFFF;
        if (value == 0) {
            return cluster;
        }
    }
    return 0;
}

int fat32_read_file(struct FAT32_BPB* bpb, uint32_t start_cluster, uint8_t* out_buffer, uint32_t bytes_to_read) {
    uint32_t current_cluster = start_cluster;
    uint32_t data_start_lba = bpb->hidden_sectors + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat_32);
    uint32_t cluster_size = bpb->sectors_per_cluster * bpb->bytes_per_sector;
    uint32_t bytes_read = 0;

    while (current_cluster < 0x0FFFFFF8 && bytes_read < bytes_to_read) {
        uint32_t target_lba = data_start_lba + ((current_cluster - 2) * bpb->sectors_per_cluster);
        uint32_t chunk_size = (bytes_to_read - bytes_read > cluster_size) ? cluster_size : (bytes_to_read - bytes_read);

        if (chunk_size == cluster_size) {
            if (disk_read(target_lba, bpb->sectors_per_cluster, out_buffer + bytes_read) != 0) return -1;
        } else {
            uint8_t cluster_buffer[512 * 32];
            if (disk_read(target_lba, bpb->sectors_per_cluster, cluster_buffer) != 0) return -1;
            for (uint32_t i = 0; i < chunk_size; i++) {
                (out_buffer + bytes_read)[i] = cluster_buffer[i];
            }
        }

        bytes_read += chunk_size;
        current_cluster = get_next_cluster(bpb, current_cluster);
    }
    return 0;
}

int fat32_write_file(struct FAT32_BPB* bpb, uint32_t* start_cluster, const uint8_t* in_buffer, uint32_t bytes_to_write) {
    uint32_t data_start_lba = bpb->hidden_sectors + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat_32);
    uint32_t cluster_size = bpb->sectors_per_cluster * bpb->bytes_per_sector;
    uint32_t bytes_written = 0;
    uint32_t current_cluster = *start_cluster;
    uint32_t prev_cluster = 0;

    if (current_cluster == 0) {
        current_cluster = find_free_cluster(bpb);
        if (current_cluster == 0) return -1;
        *start_cluster = current_cluster;
        if (set_next_cluster(bpb, current_cluster, 0x0FFFFFFF) != 0) return -1;
    }

    while (bytes_written < bytes_to_write) {
        uint32_t target_lba = data_start_lba + ((current_cluster - 2) * bpb->sectors_per_cluster);
        uint32_t chunk_size = (bytes_to_write - bytes_written > cluster_size) ? cluster_size : (bytes_to_write - bytes_written);

        if (chunk_size == cluster_size) {
            if (disk_write(target_lba, bpb->sectors_per_cluster, in_buffer + bytes_written) != 0) return -1;
        } else {
            uint8_t cluster_buffer[512 * 32];
            if (disk_read(target_lba, bpb->sectors_per_cluster, cluster_buffer) != 0) return -1;
            for (uint32_t i = 0; i < chunk_size; i++) {
                cluster_buffer[i] = (in_buffer + bytes_written)[i];
            }
            if (disk_write(target_lba, bpb->sectors_per_cluster, cluster_buffer) != 0) return -1;
        }

        bytes_written += chunk_size;
        prev_cluster = current_cluster;

        if (bytes_written < bytes_to_write) {
            uint32_t next = get_next_cluster(bpb, current_cluster);
            if (next >= 0x0FFFFFF8) {
                uint32_t new_cluster = find_free_cluster(bpb);
                if (new_cluster == 0) return -1;
                if (set_next_cluster(bpb, prev_cluster, new_cluster) != 0) return -1;
                if (set_next_cluster(bpb, new_cluster, 0x0FFFFFFF) != 0) return -1;
                current_cluster = new_cluster;
            } else {
                current_cluster = next;
            }
        }
    }
    return 0;
}

int fat32_create_dir(struct FAT32_BPB* bpb, uint32_t parent_cluster, const char* dir_name) {
    uint32_t data_start_lba = bpb->hidden_sectors + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat_32);
    uint32_t current_cluster = parent_cluster;
    
    uint32_t new_dir_cluster = find_free_cluster(bpb);
    if (new_dir_cluster == 0) return -1;
    if (set_next_cluster(bpb, new_dir_cluster, 0x0FFFFFFF) != 0) return -1;

    uint8_t zero_buffer[512] = {0};
    uint32_t new_dir_lba = data_start_lba + ((new_dir_cluster - 2) * bpb->sectors_per_cluster);
    for (uint32_t i = 0; i < bpb->sectors_per_cluster; i++) {
        if (disk_write(new_dir_lba + i, 1, zero_buffer) != 0) return -1;
    }

    struct FAT32_DirectoryEntry dot_entries[2] = {0};
    
    for (int i = 0; i < 11; i++) {
        dot_entries[0].name[i] = (i == 0) ? '.' : ' ';
        dot_entries[1].name[i] = (i < 2) ? '.' : ' ';
    }
    dot_entries[0].attributes = 0x10;
    dot_entries[1].attributes = 0x10;
    
    dot_entries[0].cluster_low = new_dir_cluster & 0xFFFF;
    dot_entries[0].cluster_high = (new_dir_cluster >> 16) & 0xFFFF;
    
    uint32_t actual_parent = (parent_cluster == bpb->root_cluster) ? 0 : parent_cluster;
    dot_entries[1].cluster_low = actual_parent & 0xFFFF;
    dot_entries[1].cluster_high = (actual_parent >> 16) & 0xFFFF;

    if (disk_write(new_dir_lba, 1, dot_entries) != 0) return -1;

    uint8_t sector_buffer[512];
    while (current_cluster < 0x0FFFFFF8) {
        uint32_t dir_lba = data_start_lba + ((current_cluster - 2) * bpb->sectors_per_cluster);
        
        for (uint32_t s = 0; s < bpb->sectors_per_cluster; s++) {
            if (disk_read(dir_lba + s, 1, sector_buffer) != 0) return -1;
            struct FAT32_DirectoryEntry* entries = (struct FAT32_DirectoryEntry*)sector_buffer;
            
            for (uint32_t e = 0; e < bpb->bytes_per_sector / sizeof(struct FAT32_DirectoryEntry); e++) {
                if (entries[e].name[0] == 0x00 || entries[e].name[0] == (char)0xE5) {
                    for (int i = 0; i < 11; i++) entries[e].name[i] = dir_name[i];
entries[e].attributes = 0x10;entries[e].reservedNT = 0;entries[e].cluster_low = new_dir_cluster & 0xFFFF;entries[e].cluster_high = (new_dir_cluster >> 16) & 0xFFFF;entries[e].file_size = 0;if (disk_write(dir_lba + s, 1, sector_buffer) != 0) return -1;return 0;}}}uint32_t next = get_next_cluster(bpb, current_cluster);if (next >= 0x0FFFFFF8) {uint32_t alloc_cluster = find_free_cluster(bpb);if (alloc_cluster == 0) return -1;if (set_next_cluster(bpb, current_cluster, alloc_cluster) != 0) return -1;if (set_next_cluster(bpb, alloc_cluster, 0x0FFFFFFF) != 0) return -1;uint32_t alloc_lba = data_start_lba + ((alloc_cluster - 2) * bpb->sectors_per_cluster);for (uint32_t i = 0; i < bpb->sectors_per_cluster; i++) {if (disk_write(alloc_lba + i, 1, zero_buffer) != 0) return -1;}current_cluster = alloc_cluster;} else {current_cluster = next;}}return -1;}