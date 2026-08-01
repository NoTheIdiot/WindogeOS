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
    return fs_create(filename);
}

int fs_read(char* filename, char* output_buffer) {
    static uint8_t output_u8[512];

    int inode_idx = find_inode_by_name(filename, NULL, NULL, NULL);
    
        if (inode_idx < 0) {
            return 0;
            log("vfs: file not found.");
        } else {
            int buffer_size = sizeof(output_u8) - 1;
            int bytes_read = sfs_read(inode_idx, output_u8, buffer_size);
            
            if (bytes_read < 0) {
                log("vfs: error during reading file.");
            } else {
                output_u8[bytes_read] = '\0';
                output_buffer = (char*)output_u8;
                return 1;
            }
        }
}

int fs_write(char* filename, char* input_buffer) {
    static uint8_t input_u8[512];
    static char input_char[512];

    size_t input_length = str_strlen(input_buffer);
    for (size_t i = 0; i < input_length; i++) {
        input_u8[i] = str_chartou8(input_char[i]);
    }

    return sfs_write(filename, input_u8, (uint32_t)input_length);
}

int fs_list_dir(void) {
    return sfs_list_directory();
}