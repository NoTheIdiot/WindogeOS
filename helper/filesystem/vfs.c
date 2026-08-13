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

int fs_delete(char* filename) {
    return sfs_delete(filename);
}

int fs_delete_last_line(char* filename) {
    return sfs_delete_last_line(filename);
}

int fs_read(char* filename, char* output_buffer, uint32_t max_size) {
    uint8_t output_u8[6144];

    if (max_size == 0) {
        return -1;
    }

    int inode_idx = find_inode_by_name(filename, NULL, NULL, NULL);
    if (inode_idx < 0) {
        log("vfs: file not found.");
        return -1;
    }

    uint32_t buffer_size = (uint32_t)sizeof(output_u8);
    int bytes_read = sfs_read(inode_idx, output_u8, buffer_size);
    if (bytes_read < 0) {
        log("vfs: error during reading file.");
        return -1;
    }

    uint32_t copy_size = (uint32_t)bytes_read;
    if (copy_size >= max_size) {
        copy_size = max_size - 1;
    }

    uint32_t out_index = 0;
    for (uint32_t i = 0; i < copy_size; i++) {
        char c = (char)output_u8[i];
        if (c == '\n') {
            output_buffer[out_index++] = '\0';
        } else {
            output_buffer[out_index++] = c;
        }
    }

    output_buffer[out_index] = '\0';
    return (int)out_index;
}

int fs_write(char* filename, char* input_buffer) {
    if (!filename || filename[0] == '\0') {
        log("vfs: invalid filename for write.");
        return -1;
    }

    if (!input_buffer || input_buffer[0] == '\0') {
        log("vfs: invalid input buffer for write.");
        return -1;
    }

    int inode_idx = find_inode_by_name(filename, NULL, NULL, NULL);
    if (inode_idx < 0) {
        log("vfs: file not found for write.");
        return -2;
    }

    uint8_t input_u8[512];
    int existing_size = sfs_read(inode_idx, input_u8, (uint32_t)sizeof(input_u8));
    if (existing_size < 0) {
        existing_size = 0;
    }

    size_t input_length = 0;
    while (input_buffer[input_length] != '\0' && input_length < sizeof(input_u8) - 1) {
        input_length++;
    }

    if ((uint32_t)existing_size + input_length + 1 > sizeof(input_u8)) {
        if ((uint32_t)existing_size >= sizeof(input_u8)) {
            log("vfs: file is full, cannot append.");
            return 0;
        }
        input_length = sizeof(input_u8) - 1 - (uint32_t)existing_size;
    }

    for (size_t i = 0; i < input_length; i++) {
        input_u8[i] = str_chartou8(input_buffer[i]);
    }
    input_u8[input_length] = str_chartou8('\n');
    size_t write_length = input_length + 1;

    int append_result = sfs_append(filename, input_u8, (uint32_t)write_length);
    if (append_result == 0) {
        return 0;
    }
    if (append_result < 0) {
        return -1;
    }
    return 1;
}

int fs_exists(char* filename) {
    if (!filename || filename[0] == '\0') {
        return 0;
    }

    return find_inode_by_name(filename, NULL, NULL, NULL) >= 0 ? 1 : 0;
}

int fs_list_dir(int hidden) {
    return sfs_list_directory(hidden);
}

int fs_rename(char* filename, char* newname) {
    if (!filename || filename[0] == '\0' || !newname || newname[0] == '\0') {
        dogeio_text_println("Error: invalid rename arguments.");
        return 0;
    }

    if (!fs_exists(filename)) {
        dogeio_text_println("Error: source file does not exist.");
        return 0;
    }

    if (fs_exists(newname)) {
        dogeio_text_println("Error: target file already exists.");
        return 0;
    }

    if (fs_create(newname) < 0) {
        dogeio_text_println("Error: unable to create target file.");
        return 0;
    }

    uint8_t raw_buffer[DIRECT_POINTERS * BLOCK_SIZE];
    int inode_idx = find_inode_by_name(filename, NULL, NULL, NULL);
    if (inode_idx < 0) {
        dogeio_text_println("Error: unable to locate source file.");
        return 0;
    }

    int bytes_read = sfs_read(inode_idx, raw_buffer, sizeof(raw_buffer));
    if (bytes_read < 0) {
        dogeio_text_println("Error: unable to read source file.");
        return 0;
    }

    int write_result = sfs_write(newname, raw_buffer, (uint32_t)bytes_read);
    if (write_result != 1) {
        dogeio_text_println("Error: unable to write renamed file.");
        return 0;
    }

    if (!fs_delete(filename)) {
        dogeio_text_println("Warning: source file still exists after rename.");
    }

    return 1;
}

void fs_copy(char* source, char* dest) {
    uint8_t raw_buffer[DIRECT_POINTERS * BLOCK_SIZE];
    
    int inode_idx = find_inode_by_name(source, NULL, NULL, NULL);
    if (inode_idx < 0) {
        log("vfs: copy source file not found.");
        return;
    }

    int bytes_read = sfs_read(inode_idx, raw_buffer, sizeof(raw_buffer));
    if (bytes_read < 0) {
        log("vfs: error reading source for copy.");
        return;
    }

    if (fs_exists(dest)) {
        fs_delete(dest);
    }
    fs_create(dest);

    sfs_write(dest, raw_buffer, (uint32_t)bytes_read);
}