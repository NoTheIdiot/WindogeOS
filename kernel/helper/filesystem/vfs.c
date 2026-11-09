#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <system.h>

static uint32_t g_block_size = 1024;

int fs_mount(uint8_t drive) {
    sprintln("[VFS] Mounting filesystem on drive...");
    int result = ext2_mount(drive);
    if (result < 0) {
        sprintln("[VFS Error] Failed to mount filesystem.");
    } else {
        sprintln("[VFS] Filesystem mounted successfully.");
    }
    return result;
}

int fs_read(const char *filename, char *text, uint32_t max_len) {
    sprintln("[VFS] Reading text file...");
    uint32_t inode_num = ext2_lookup(filename);
    if (inode_num == 0) {
        sprintln("[VFS Error] File not found for reading.");
        return -1;
    }

    struct ext2_inode file_inode;
    if (ext2_read_inode(inode_num, &file_inode) != 0) {
        sprintln("[VFS Error] Failed to read inode for file.");
        return -1;
    }

    uint32_t bytes_to_read = file_inode.i_size;
    if (bytes_to_read > max_len - 1) {
        bytes_to_read = max_len - 1;
    }

    uint8_t *block_buf = __builtin_alloca(g_block_size);
    uint32_t sectors_per_block = g_block_size / 512;
    uint64_t lba = (uint64_t)file_inode.i_block[0] * sectors_per_block;

    for (uint32_t i = 0; i < sectors_per_block; i++) {
        if (ata_read_sector(ata_current_drive(), lba + i, block_buf + (i * 512)) != 0) {
            sprintln("[VFS Error] ATA sector read failed during text read.");
            return -1;
        }
    }

    memcpy(text, block_buf, bytes_to_read);
    text[bytes_to_read] = '\0';

    sprintln("[VFS] Text file read completed.");
    return (int)bytes_to_read;
}

int fs_write(const char *filename, const char *text, uint32_t len) {
    sprintln("[VFS] Writing text file...");
    uint32_t inode_num = ext2_lookup(filename);
    if (inode_num == 0) {
        sprintln("[VFS Error] File not found for writing.");
        return -1;
    }

    struct ext2_inode file_inode;
    if (ext2_read_inode(inode_num, &file_inode) != 0) {
        sprintln("[VFS Error] Failed to read inode for writing.");
        return -1;
    }

    if (len > g_block_size) {
        len = g_block_size;
    }

    uint8_t *block_buf = __builtin_alloca(g_block_size);
    memset(block_buf, 0, g_block_size);
    memcpy(block_buf, text, len);

    uint32_t sectors_per_block = g_block_size / 512;
    uint64_t lba = (uint64_t)file_inode.i_block[0] * sectors_per_block;

    for (uint32_t i = 0; i < sectors_per_block; i++) {
        if (ata_write_sector(ata_current_drive(), lba + i, block_buf + (i * 512)) != 0) {
            sprintln("[VFS Error] ATA sector write failed during text write.");
            return -1;
        }
    }

    file_inode.i_size = len;
    if (ext2_write_inode(inode_num, &file_inode) != 0) {
        sprintln("[VFS Error] Failed to update inode size after text write.");
        return -1;
    }

    sprintln("[VFS] Text file write completed.");
    return (int)len;
}

int fs_read_raw(const char *filename, uint8_t *buf, uint32_t max_len) {
    sprintln("[VFS] Performing raw file read...");
    uint32_t inode_num = ext2_lookup(filename);
    if (inode_num == 0) {
        sprintln("[VFS Error] File not found for raw read.");
        return -1;
    }

    struct ext2_inode file_inode;
    if (ext2_read_inode(inode_num, &file_inode) != 0) {
        sprintln("[VFS Error] Failed to read inode for raw read.");
        return -1;
    }

    uint32_t bytes_to_read = file_inode.i_size;
    if (bytes_to_read > max_len) {
        bytes_to_read = max_len;
    }

    uint32_t total_read = 0;
    uint32_t sectors_per_block = g_block_size / 512;

    for (uint32_t i = 0; i < 12 && total_read < bytes_to_read; i++) {
        uint32_t block = file_inode.i_block[i];
        if (block == 0) {
            break;
        }

        uint32_t chunk = g_block_size;
        if (total_read + chunk > bytes_to_read) {
            chunk = bytes_to_read - total_read;
        }

        uint8_t *block_buf = __builtin_alloca(g_block_size);
        uint64_t lba = (uint64_t)block * sectors_per_block;

        for (uint32_t s = 0; s < sectors_per_block; s++) {
            if (ata_read_sector(ata_current_drive(), lba + s, block_buf + (s * 512)) != 0) {
                sprintln("[VFS Error] ATA sector read failed during raw read.");
                return -1;
            }
        }

        memcpy(buf + total_read, block_buf, chunk);
        total_read += chunk;
    }

    sprintln("[VFS] Raw file read completed.");
    return (int)total_read;
}

int fs_write_raw(const char *filename, const uint8_t *buf, uint32_t len) {
    sprintln("[VFS] Performing raw file write...");
    uint32_t inode_num = ext2_lookup(filename);
    if (inode_num == 0) {
        sprintln("[VFS Error] File not found for raw write.");
        return -1;
    }

    struct ext2_inode file_inode;
    if (ext2_read_inode(inode_num, &file_inode) != 0) {
        sprintln("[VFS Error] Failed to read inode for raw write.");
        return -1;
    }

    uint32_t sectors_per_block = g_block_size / 512;
    uint32_t total_written = 0;

    for (uint32_t i = 0; i < 12 && total_written < len; i++) {
        uint32_t block = file_inode.i_block[i];
        if (block == 0) {
            break;
        }

        uint32_t chunk = g_block_size;
        if (total_written + chunk > len) {
            chunk = len - total_written;
        }

        uint8_t *block_buf = __builtin_alloca(g_block_size);
        memset(block_buf, 0, g_block_size);
        memcpy(block_buf, buf + total_written, chunk);

        uint64_t lba = (uint64_t)block * sectors_per_block;

        for (uint32_t s = 0; s < sectors_per_block; s++) {
            if (ata_write_sector(ata_current_drive(), lba + s, block_buf + (s * 512)) != 0) {
                sprintln("[VFS Error] ATA sector write failed during raw write.");
                return -1;
            }
        }

        total_written += chunk;
    }

    file_inode.i_size = len;
    if (ext2_write_inode(inode_num, &file_inode) != 0) {
        sprintln("[VFS Error] Failed to update inode size after raw write.");
        return -1;
    }

    sprintln("[VFS] Raw file write completed.");
    return (int)total_written;
}

int fs_read_raw_at(const char *filename, uint8_t *buf, uint32_t offset, uint32_t len) {
    sprintln("[VFS] Reading raw data at offset...");
    uint32_t inode_num = ext2_lookup(filename);
    if (inode_num == 0) {
        sprintln("[VFS Error] File not found for raw read at offset.");
        return -1;
    }

    struct ext2_inode file_inode;
    if (ext2_read_inode(inode_num, &file_inode) != 0) {
        sprintln("[VFS Error] Failed to read inode for raw read at offset.");
        return -1;
    }

    if (offset >= file_inode.i_size) {
        return 0;
    }

    uint32_t bytes_to_read = len;
    if (offset + bytes_to_read > file_inode.i_size) {
        bytes_to_read = file_inode.i_size - offset;
    }

    uint32_t block_index = offset / g_block_size;
    uint32_t block_offset = offset % g_block_size;

    if (block_index >= 12) {
        sprintln("[VFS Error] Block index exceeds direct blocks limit.");
        return -1;
    }

    uint32_t target_block = file_inode.i_block[block_index];
    if (target_block == 0) {
        return 0;
    }

    uint8_t *block_buf = __builtin_alloca(g_block_size);
    uint32_t sectors_per_block = g_block_size / 512;
    uint64_t lba = (uint64_t)target_block * sectors_per_block;

    for (uint32_t i = 0; i < sectors_per_block; i++) {
        if (ata_read_sector(ata_current_drive(), lba + i, block_buf + (i * 512)) != 0) {
            sprintln("[VFS Error] ATA sector read failed during raw read at offset.");
            return -1;
        }
    }

    uint32_t chunk = g_block_size - block_offset;
    if (bytes_to_read > chunk) {
        bytes_to_read = chunk;
    }

    memcpy(buf, block_buf + block_offset, bytes_to_read);
    sprintln("[VFS] Raw read at offset completed.");
    return (int)bytes_to_read;
}

int fs_write_raw_at(const char *filename, const uint8_t *buf, uint32_t offset, uint32_t len) {
    sprintln("[VFS] Writing raw data at offset...");
    uint32_t inode_num = ext2_lookup(filename);
    if (inode_num == 0) {
        sprintln("[VFS Error] File not found for raw write at offset.");
        return -1;
    }

    struct ext2_inode file_inode;
    if (ext2_read_inode(inode_num, &file_inode) != 0) {
        sprintln("[VFS Error] Failed to read inode for raw write at offset.");
        return -1;
    }

    uint32_t block_index = offset / g_block_size;
    uint32_t block_offset = offset % g_block_size;

    if (block_index >= 12) {
        sprintln("[VFS Error] Block index exceeds direct blocks limit.");
        return -1;
    }

    uint32_t target_block = file_inode.i_block[block_index];
    if (target_block == 0) {
        sprintln("[VFS Error] Target block is unallocated.");
        return -1;
    }

    uint8_t *block_buf = __builtin_alloca(g_block_size);
    uint32_t sectors_per_block = g_block_size / 512;
    uint64_t lba = (uint64_t)target_block * sectors_per_block;

    for (uint32_t i = 0; i < sectors_per_block; i++) {
        if (ata_read_sector(ata_current_drive(), lba + i, block_buf + (i * 512)) != 0) {
            sprintln("[VFS Error] ATA sector read failed during raw write at offset.");
            return -1;
        }
    }

    uint32_t chunk = g_block_size - block_offset;
    uint32_t bytes_to_write = len;
    if (bytes_to_write > chunk) {
        bytes_to_write = chunk;
    }

    memcpy(block_buf + block_offset, buf, bytes_to_write);

    for (uint32_t i = 0; i < sectors_per_block; i++) {
        if (ata_write_sector(ata_current_drive(), lba + i, block_buf + (i * 512)) != 0) {
            sprintln("[VFS Error] ATA sector write failed during raw write at offset.");
            return -1;
        }
    }

    uint32_t new_size = offset + bytes_to_write;
    if (new_size > file_inode.i_size) {
        file_inode.i_size = new_size;
        if (ext2_write_inode(inode_num, &file_inode) != 0) {
            sprintln("[VFS Error] Failed to update inode size after raw write at offset.");
            return -1;
        }
    }

    sprintln("[VFS] Raw write at offset completed.");
    return (int)bytes_to_write;
}

int fs_create(const char *filename, bool is_directory) {
    sprintln("[VFS] Creating new file or directory...");
    uint16_t mode = is_directory ? 0x41ED : 0x81ED;
    int result = ext2_create_file(filename, mode);
    if (result < 0) {
        sprintln("[VFS Error] Failed to create file/directory.");
    } else {
        sprintln("[VFS] File/directory created successfully.");
    }
    return result;
}

bool fs_exists(const char *filename) {
    sprintln("[VFS] Checking if file/directory exists...");
    uint32_t inode_num = ext2_lookup(filename);
    if (inode_num != 0) {
        sprintln("[VFS] File/directory exists.");
        return true;
    } else {
        sprintln("[VFS] File/directory does not exist.");
        return false;
    }
}

int fs_list_root_dir(void) {
    sprintln("[VFS] Listing root directory entries...");
    return ext2_list_root_dir();
}

int fs_list(fs_t *out_list, int max_entries) {
    return ext2_list(out_list, max_entries);
}