/*
a vfs so everything is easier later
*/

#include <boot/kernel.h>
#include <dogeio.h>
#include <basicutil.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

int fs_format(void) {
    return sfs_format();
}

int fs_create(char* filename) {
    return sfs_create(filename);
}

int fs_read(char* filename, char* output_buffer) {
    uint8_t output_u8[512];

    int inode_idx = find_inode_by_name(filename, NULL, NULL, NULL);
    if (inode_idx < 0) {
        log("vfs: file not found.\n");
        return -1;
    }

    int buffer_size = (int)sizeof(output_u8) - 1;
    int bytes_read = sfs_read(inode_idx, output_u8, (uint32_t)buffer_size);
    if (bytes_read < 0) {
        log("vfs: error during reading file.\n");
        return -1;
    }

    output_u8[bytes_read] = '\0';
    memcpy(output_buffer, output_u8, (size_t)bytes_read + 1);
    return bytes_read;
}

int fs_write(char* filename, char* input_buffer) {
    if (!filename || filename[0] == '\0') {
        log("vfs: invalid filename for write.\n");
        return -1;
    }

    int inode_idx = find_inode_by_name(filename, NULL, NULL, NULL);
    if (inode_idx < 0) {
        if (sfs_create(filename) < 0) {
            log("vfs: cannot create file for write.\n");
            return -1;
        }
    }

    uint8_t input_u8[512];
    size_t input_length = str_strlen(input_buffer);
    if (input_length > sizeof(input_u8)) {
        input_length = sizeof(input_u8);
    }

    for (size_t i = 0; i < input_length; i++) {
        input_u8[i] = str_chartou8(input_buffer[i]);
    }

    return sfs_write(filename, input_u8, (uint32_t)input_length);
}

int fs_list_dir(void) {
    return sfs_list_directory();
}