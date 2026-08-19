#include <boot/kernel.h>
#include <dogeio.h>
#include <basicutil.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

int fs_format(void) {
    return exfat_wipe_and_format();
}

int fs_create(char* filename) {
    return exfat_create_node(filename, false);
}

int fs_mkdir(char* foldername) {
    return exfat_create_node(foldername, true); 
}

int fs_exists(char* filename) {
    if (!filename || filename[0] == '\0') {
        return 0;
    }
    uint8_t dummy[1];
    return exfat_read_file(filename, dummy, 0) >= 0 ? 1 : 0;
}

int fs_delete(char* filename) {
    if (!fs_exists(filename)) {
        dogeio_text_println("fs (delete): file does not exist");
        return 0;
    }

    if (exfat_delete_node(filename) == 0) {
        dogeio_text_println("fs (delete): file deleted");
        return 1;
    }

    dogeio_text_println("fs (delete): error deleting file");
    return -1;
}

int fs_delete_last_line(char* filename) {
    return exfat_truncate_last_line(filename);
}

int fs_read(char* filename, char* output_buffer, uint32_t max_size) {
    if (max_size == 0 || !output_buffer) {
        return -1;
    }

    uint8_t raw_buffer[4096];
    int bytes_read = exfat_read_file(filename, raw_buffer, sizeof(raw_buffer));
    if (bytes_read < 0) {
        dogeio_text_println("vfs: file not found or read error.");
        return -1;
    }

    uint32_t copy_size = (uint32_t)bytes_read;
    if (copy_size >= max_size) {
        copy_size = max_size - 1;
    }

    uint32_t out_index = 0;
    for (uint32_t i = 0; i < copy_size; i++) {
        char c = (char)raw_buffer[i];
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
        dogeio_text_println("vfs: invalid filename for write.");
        return -1;
    }

    if (!input_buffer || input_buffer[0] == '\0') {
        dogeio_text_println("vfs: invalid input buffer for write.");
        return -1;
    }

    if (!fs_exists(filename)) {
        dogeio_text_println("vfs: file not found for write.");
        return -2;
    }

    uint8_t input_u8[512];
    size_t input_length = strlen(input_buffer);
    if (input_length > sizeof(input_u8) - 2) {
        input_length = sizeof(input_u8) - 2;
    }

    memcpy(input_u8, input_buffer, input_length);
    input_u8[input_length] = '\n';
    size_t write_length = input_length + 1;

    int append_result = exfat_append_file(filename, input_u8, (uint64_t)write_length);
    if (append_result == 0) {
        return 0;
    }
    return -1;
}

int fs_list_dir(int hidden) {
    (void)hidden;
    return exfat_print_directory();
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

    uint8_t raw_buffer[4096];
    int bytes_read = exfat_read_file(filename, raw_buffer, sizeof(raw_buffer));
    if (bytes_read < 0) {
        dogeio_text_println("Error: unable to read source file.");
        return 0;
    }

    int write_result = exfat_write_file(newname, raw_buffer, (uint64_t)bytes_read);
    if (write_result != 0) {
        dogeio_text_println("Error: unable to write renamed file.");
        return 0;
    }

    if (!fs_delete(filename)) {
        dogeio_text_println("Warning: source file still exists after rename.");
    }

    return 1;
}

void fs_copy(char* source, char* dest) {
    uint8_t raw_buffer[4096];
    int bytes_read = exfat_read_file(source, raw_buffer, sizeof(raw_buffer));
    if (bytes_read < 0) {
        dogeio_text_println("vfs: copy source file not found or unreadable.");
        return;
    }

    if (fs_exists(dest)) {
        fs_delete(dest);
    }
    fs_create(dest);

    exfat_write_file(dest, raw_buffer, (uint64_t)bytes_read);
}

int fs_chdir(char* folder) {
    return exfat_change_directory(folder);
}

char* fs_dirname(void) {
    return (char*)exfat_get_working_dir();
}