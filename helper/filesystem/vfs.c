/*
a vfs so everything is easier later
*/

#include <boot/kernel.h>
#include <dogeio.h>
#include <stdint.h>
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
            dogeio_text_println("File not found!");
        } else {
            int bytes_read = fs_read(inode_idx, , sizeof(output) - 1);
            
            if (bytes_read < 0) {
                dogeio_text_println("Error reading file.");
            } else {
                output[bytes_read] = '\0';
                dogeio_text_println((char*)output);
            }
        }
}