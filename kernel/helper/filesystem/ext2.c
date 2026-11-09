#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <system.h>
#include <dogeio.h> 

#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_ROOT_INODE 2
#define PARTITION_START_LBA 4096

static uint32_t g_block_size = 1024;
static uint32_t g_inode_table_block = 0;
static uint32_t g_inode_bitmap_block = 0;
static uint32_t g_block_bitmap_block = 0;
static uint32_t g_inodes_count = 0;
static uint32_t g_blocks_count = 0;
static uint8_t g_ext2_drive = 0;

// Static scratch buffers to prevent stack exhaustion and page faults
static uint8_t g_scratch_block_1[4096];
static uint8_t g_scratch_block_2[4096];
static uint8_t g_scratch_block_3[4096];

static int ext2_read_blocks(uint32_t block_num, uint32_t count, uint8_t *buf) {
    uint32_t sectors_per_block = g_block_size / 512;
    uint64_t lba = PARTITION_START_LBA + ((uint64_t)block_num * sectors_per_block);
    uint32_t total_sectors = count * sectors_per_block;
    
    for (uint32_t i = 0; i < total_sectors; i++) {
        if (ata_read_sector(g_ext2_drive, lba + i, buf + (i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

static int ext2_write_blocks(uint32_t block_num, uint32_t count, const uint8_t *buf) {
    uint32_t sectors_per_block = g_block_size / 512;
    uint64_t lba = PARTITION_START_LBA + ((uint64_t)block_num * sectors_per_block);
    uint32_t total_sectors = count * sectors_per_block;
    
    for (uint32_t i = 0; i < total_sectors; i++) {
        if (ata_write_sector(g_ext2_drive, lba + i, buf + (i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

void test_ata_persistence(void) {
    uint8_t test_buf_write[512];
    uint8_t test_buf_read[512];
    
    for(int i=0; i<512; i++) test_buf_write[i] = 0xAA;
    memset(test_buf_read, 0, 512);

    uint64_t test_lba = 5000;

    sprintln("[ATA Test] Writing test pattern to LBA 5000...");
    if (ata_write_sector(g_ext2_drive, test_lba, test_buf_write) != 0) {
        sprintln("[ATA Test Error] ata_write_sector returned failure!");
        return;
    }

    sprintln("[ATA Test] Reading back LBA 5000...");
    if (ata_read_sector(g_ext2_drive, test_lba, test_buf_read) != 0) {
        sprintln("[ATA Test Error] ata_read_sector returned failure!");
        return;
    }

    if (test_buf_read[0] == 0xAA && test_buf_read[511] == 0xAA) {
        sprintln("[ATA Test SUCCESS] Read-after-write matched! ATA writes are working.");
    } else {
        sprintln("[ATA Test FAIL] Read-after-write MISMATCH! ATA write did not persist to disk.");
    }
}

static void ext2_reset_cache_state(void) {
    g_block_size = 1024;
    g_inode_table_block = 0;
    g_inode_bitmap_block = 0;
    g_block_bitmap_block = 0;
    g_inodes_count = 0;
    g_blocks_count = 0;
}

int ext2_mount(uint8_t drive) {
    ext2_reset_cache_state();
    g_ext2_drive = drive;
    uint8_t sb_buf[1024];

    uint64_t sb_lba_1 = PARTITION_START_LBA + 2;
    uint64_t sb_lba_2 = PARTITION_START_LBA + 3;

    sprintln("[Ext2 Debug] Reading superblock from LBA 4098 & 4099...");

    if (ata_read_sector(drive, sb_lba_1, sb_buf) != 0) {
        sprintln("[Ext2 Error] Mount failed to read SB LBA 1");
        return -1;
    }
    if (ata_read_sector(drive, sb_lba_2, sb_buf + 512) != 0) {
        sprintln("[Ext2 Error] Mount failed to read SB LBA 2");
        return -1;
    }

    uint8_t m1 = sb_buf[56];
    uint8_t m2 = sb_buf[57];
    sprintln("[Ext2 Debug] Raw bytes read at superblock magic offset (56-57):");
    if (m1 == 0x53 && m2 == 0xef) {
        sprintln("[Ext2 Debug] Magic bytes MATCH 0x53EF in memory for god sake");
    } else {
        sprintln("[Ext2 Error] Magic bytes do NOT match fuck Read different bytes.");
    }

    struct ext2_superblock *sb = (struct ext2_superblock *)sb_buf;

    g_block_size = 1024U << sb->s_log_block_size;
    g_inodes_count = sb->s_inodes_count;
    g_blocks_count = sb->s_blocks_count;

    uint32_t bgd_block = (g_block_size == 1024) ? 2 : 1;
    uint8_t bgd_buf[1024];
    if (ext2_read_blocks(bgd_block, 1, bgd_buf) != 0) {
        sprintln("[Ext2 Error] Mount failed to read BGD block");
        return -1;
    }

    char num_buf[32];
    sprintln("[Ext2 Debug] BGD block first 16 bytes:");
    for (int i = 0; i < 16; i++) {
        uint64_to_str(bgd_buf[i], num_buf);
        sprint(num_buf);
        sprint(" ");
    }
    sprintln("");

    struct ext2_bgd *bgd = (struct ext2_bgd *)bgd_buf;
    g_inode_bitmap_block = bgd->bg_inode_bitmap;
    g_block_bitmap_block = bgd->bg_block_bitmap;
    g_inode_table_block = bgd->bg_inode_table;

    sprint("[Ext2 Debug] Inode Bitmap Block: ");
    uint64_to_str(g_inode_bitmap_block, num_buf);
    sprintln(num_buf);

    sprint("[Ext2 Debug] Block Bitmap Block: ");
    uint64_to_str(g_block_bitmap_block, num_buf);
    sprintln(num_buf);

    sprint("[Ext2 Debug] Inode Table Block: ");
    uint64_to_str(g_inode_table_block, num_buf);
    sprintln(num_buf);

    sprintln("[Ext2 Debug] Mount successful. Bitmaps and tables fully loaded.");
    return 1;
}

static uint32_t ext2_alloc_inode(void) {
    sprintln("[Ext2 Debug] Entering ext2_alloc_inode");
    
    if (g_inode_bitmap_block == 0) {
        sprintln("[Ext2 Error] g_inode_bitmap_block is somehow zero...");
        return 0;
    }

    if (ext2_read_blocks(g_inode_bitmap_block, 1, g_scratch_block_1) != 0) {
        sprintln("[Ext2 Error] Failed to read inode bitmap block");
        return 0;
    }

    sprintln("[Ext2 Debug] Inode bitmap read successfully");

    for (uint32_t i = 0; i < g_inodes_count; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (!(g_scratch_block_1[byte_idx] & (1 << bit_idx))) {
            g_scratch_block_1[byte_idx] |= (1 << bit_idx);
            if (ext2_write_blocks(g_inode_bitmap_block, 1, g_scratch_block_1) != 0) {
                sprintln("[Ext2 Error] Failed to write back inode bitmap");
                return 0;
            }
            sprintln("[Ext2 Debug] Inode allocated successfully");
            return i + 1;
        }
    }
    
    sprintln("[Ext2 Error] No free inodes found in bitmap, fu-");
    return 0;
}

int ext2_read_inode(uint32_t inode_num, struct ext2_inode *out_inode) {
    uint32_t inodes_per_block = g_block_size / sizeof(struct ext2_inode);
    uint32_t block_offset = (inode_num - 1) / inodes_per_block;
    uint32_t index_in_block = (inode_num - 1) % inodes_per_block;

    uint32_t target_block = g_inode_table_block + block_offset;

    if (ext2_read_blocks(target_block, 1, g_scratch_block_2) != 0) {
        return -1;
    }

    struct ext2_inode *inodes = (struct ext2_inode *)g_scratch_block_2;
    *out_inode = inodes[index_in_block];

    if (inode_num == EXT2_ROOT_INODE) {
        char num_buf[32];
        sprintln("[Ext2 Debug] Root Inode loaded:");
        
        sprint("  i_size: ");
        uint64_to_str(out_inode->i_size, num_buf);
        sprintln(num_buf);

        sprint("  i_block[0]: ");
        uint64_to_str(out_inode->i_block[0], num_buf);
        sprintln(num_buf);
    }

    return 0;
}

int ext2_write_inode(uint32_t inode_num, const struct ext2_inode *in_inode) {
    uint32_t inodes_per_block = g_block_size / sizeof(struct ext2_inode);
    uint32_t block_offset = (inode_num - 1) / inodes_per_block;
    uint32_t index_in_block = (inode_num - 1) % inodes_per_block;

    uint32_t target_block = g_inode_table_block + block_offset;
    if (ext2_read_blocks(target_block, 1, g_scratch_block_2) != 0) {
        return -1;
    }

    struct ext2_inode *inodes = (struct ext2_inode *)g_scratch_block_2;
    inodes[index_in_block] = *in_inode;

    if (ext2_write_blocks(target_block, 1, g_scratch_block_2) != 0) {
        return -1;
    }
    return 0;
}

static uint32_t ext2_alloc_block(void) {
    if (ext2_read_blocks(g_block_bitmap_block, 1, g_scratch_block_1) != 0) {
        return 0;
    }

    for (uint32_t i = 0; i < g_blocks_count; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (!(g_scratch_block_1[byte_idx] & (1 << bit_idx))) {
            g_scratch_block_1[byte_idx] |= (1 << bit_idx);
            if (ext2_write_blocks(g_block_bitmap_block, 1, g_scratch_block_1) != 0) {
                return 0;
            }
            return i + 1;
        }
    }
    return 0;
}

int ext2_create_file(const char *filename, uint16_t mode) {
    sprintln("[Ext2 Debug] Starting create_file");

    if (ext2_lookup(filename) != 0) {
        sprintln("[Ext2 Error] File already exists!");
        return -1;
    }

    uint32_t new_inode_num = ext2_alloc_inode();
    if (new_inode_num == 0) {
        sprintln("[Ext2 Error] ext2_alloc_inode failed");
        return -1;
    }
    sprintln("[Ext2 Debug] Allocated inode successfully");

    uint32_t new_block_num = ext2_alloc_block();
    if (new_block_num == 0) {
        sprintln("[Ext2 Error] ext2_alloc_block failed");
        return -1;
    }
    sprintln("[Ext2 Debug] Allocated data block successfully");

    memset(g_scratch_block_3, 0, g_block_size);
    if (ext2_write_blocks(new_block_num, 1, g_scratch_block_3) != 0) {
        sprintln("[Ext2 Error] Failed to zero-out new data block");
        return -1;
    }

    struct ext2_inode new_inode;
    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.i_mode = mode;
    new_inode.i_links_count = 1;
    new_inode.i_size = 0;
    new_inode.i_block[0] = new_block_num;

    if (ext2_write_inode(new_inode_num, &new_inode) != 0) {
        sprintln("[Ext2 Error] Failed to write new inode structure");
        return -1;
    }

    struct ext2_inode root_inode;
    if (ext2_read_inode(EXT2_ROOT_INODE, &root_inode) != 0) {
        sprintln("[Ext2 Error] Failed to read root inode");
        return -1;
    }

    uint32_t name_len = strlen(filename);
    uint32_t needed_len = 8 + ((name_len + 3) & ~3);
    uint32_t num_blocks = (root_inode.i_size + g_block_size - 1) / g_block_size;
    if (num_blocks == 0) num_blocks = 1;

    bool added = false;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t block_num = root_inode.i_block[b];
        if (block_num == 0) continue;

        if (ext2_read_blocks(block_num, 1, g_scratch_block_3) != 0) continue;

        uint32_t offset = 0;
        uint32_t last_offset = 0;
        while (offset < g_block_size) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(g_scratch_block_3 + offset);
            if (entry->rec_len == 0 || entry->rec_len > (g_block_size - offset)) break;
            last_offset = offset;
            offset += entry->rec_len;
        }

        struct ext2_dir_entry *last_entry = (struct ext2_dir_entry *)(g_scratch_block_3 + last_offset);
        uint32_t actual_len = 8 + ((last_entry->name_len + 3) & ~3);
        uint32_t slack = last_entry->rec_len - actual_len;

        if (slack >= needed_len) {
            last_entry->rec_len = actual_len;
            struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)((uint8_t *)last_entry + actual_len);
            new_entry->inode = new_inode_num;
            new_entry->rec_len = slack;
            new_entry->name_len = name_len;
            new_entry->file_type = 1;
            memcpy(new_entry->name, filename, name_len);

            if (ext2_write_blocks(block_num, 1, g_scratch_block_3) != 0) {
                sprintln("[Ext2 Error] Failed to write back updated root directory block");
                return -1;
            }
            added = true;
            break;
        }
    }

    if (!added) {
        if (num_blocks >= 12) {
            sprintln("[Ext2 Error] Root directory exceeds maximum direct blocks!");
            return -1;
        }
        uint32_t new_dir_block = ext2_alloc_block();
        if (new_dir_block == 0) {
            sprintln("[Ext2 Error] Failed to allocate new directory block");
            return -1;
        }

        root_inode.i_block[num_blocks] = new_dir_block;
        root_inode.i_size += g_block_size;
        root_inode.i_blocks += (g_block_size / 512);

        memset(g_scratch_block_3, 0, g_block_size);
        struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)g_scratch_block_3;
        new_entry->inode = new_inode_num;
        new_entry->rec_len = g_block_size;
        new_entry->name_len = name_len;
        new_entry->file_type = 1;
        memcpy(new_entry->name, filename, name_len);

        if (ext2_write_blocks(new_dir_block, 1, g_scratch_block_3) != 0) {
            sprintln("[Ext2 Error] Failed to write new directory block");
            return -1;
        }
    }

    if (ext2_write_inode(EXT2_ROOT_INODE, &root_inode) != 0) {
        sprintln("[Ext2 Error] Failed to update root inode");
        return -1;
    }

    sprintln("[Ext2 Debug] File created successfully!");
    return 0;
}

int ext2_list_root_dir(void) {
    struct ext2_inode root_inode;
    if (ext2_read_inode(EXT2_ROOT_INODE, &root_inode) != 0) {
        return -1;
    }

    if (ext2_read_blocks(root_inode.i_block[0], 1, g_scratch_block_3) != 0) {
        return -1;
    }

    uint32_t offset = 0;
    while (offset < g_block_size) {
        struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(g_scratch_block_3 + offset);
        if (entry->rec_len == 0 || entry->rec_len > (g_block_size - offset)) break;

        if (entry->inode != 0) {
            char name_buf[256];
            int len = entry->name_len;
            if (len > 255) len = 255;

            memcpy(name_buf, entry->name, len);
            name_buf[len] = '\0';
            sprintln(name_buf);
        }

        offset += entry->rec_len;
    }
    return 0;
}

uint32_t ext2_lookup(const char *filename) {
    struct ext2_inode root_inode;
    if (ext2_read_inode(EXT2_ROOT_INODE, &root_inode) != 0) {
        sprintln("[Ext2 Lookup Debug] Failed to read root inode");
        return 0;
    }

    uint32_t num_blocks = (root_inode.i_size + g_block_size - 1) / g_block_size;
    if (num_blocks == 0) num_blocks = 1;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t block_num = root_inode.i_block[b];
        if (block_num == 0) {
            continue;
        }

        if (ext2_read_blocks(block_num, 1, g_scratch_block_3) != 0) {
            continue;
        }

        uint32_t offset = 0;
        while (offset < g_block_size) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(g_scratch_block_3 + offset);
            if (entry->rec_len == 0 || entry->rec_len > (g_block_size - offset)) break;

            if (entry->inode != 0) {
                char name_buf[256];
                int len = entry->name_len;
                if (len > 255) len = 255;

                memcpy(name_buf, entry->name, len);
                name_buf[len] = '\0';

                if (strcmp(name_buf, filename) == 0) {
                    return entry->inode;
                }
            }
            offset += entry->rec_len;
        }
    }
    return 0;
}

int ext2_list(fs_t *out_list, int max_entries) {
    struct ext2_inode root_inode;
    if (ext2_read_inode(EXT2_ROOT_INODE, &root_inode) != 0) {
        return -1;
    }

    uint32_t num_blocks = (root_inode.i_size + g_block_size - 1) / g_block_size;
    if (num_blocks == 0) num_blocks = 1;

    int count = 0;

    for (uint32_t b = 0; b < num_blocks && count < max_entries; b++) {
        uint32_t block_num = root_inode.i_block[b];
        if (block_num == 0) {
            continue;
        }

        if (ext2_read_blocks(block_num, 1, g_scratch_block_3) != 0) {
            continue;
        }

        uint32_t offset = 0;
        while (offset < g_block_size && count < max_entries) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(g_scratch_block_3 + offset);
            if (entry->rec_len == 0 || entry->rec_len > (g_block_size - offset)) {
                break;
            }

            if (entry->inode != 0) {
                int len = entry->name_len;
                if (len > 255) len = 255;

                memcpy(out_list[count].name, entry->name, len);
                out_list[count].name[len] = '\0';

                struct ext2_inode target_inode;
                if (ext2_read_inode(entry->inode, &target_inode) == 0) {
                    out_list[count].size = target_inode.i_size;
                    out_list[count].is_dir = ((target_inode.i_mode & 0xF000) == 0x4000);
                } else {
                    out_list[count].size = 0;
                    out_list[count].is_dir = false;
                }

                count++;
            }
            offset += entry->rec_len;
        }
    }

    return count;
}