#include <boot/kernel.h>
#include <dogeio.h>
#include <basicutil.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <bool.h>

extern int exfat_resolve_entry(const char *target_name, void *out);

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
    if (!filename || filename[0] == '\0') return 0;
    return (exfat_resolve_entry(filename, NULL) == 0) ? 1 : 0;
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
    if (!output_buffer) return -1;

    if (!fs_exists(filename)) {
        dogeio_text_println("vfs: file not found or read error.");
        return -1;
    }

    return (int)exfat_read_file(filename, (uint8_t*)output_buffer, max_size);
}

int fs_write_bytes(char* filename, char* input_buffer, uint32_t size) {
    if (!filename || filename[0] == '\0') {
        dogeio_text_println("vfs: invalid filename for write.");
        return -1;
    }

    if (!fs_exists(filename)) {
        dogeio_text_println("vfs: file not found for write.");
        return -2;
    }

    return exfat_write_file(filename, (uint8_t*)input_buffer, (uint64_t)size);
}

int fs_write(char* filename, char* input_buffer) {
    if (!input_buffer) return -1;
    return fs_write_bytes(filename, input_buffer, (uint32_t)str_strlen(input_buffer));
}

int fs_list_dir(int hidden) {
    return exfat_print_directory(hidden);
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

    fs_copy(filename, newname);
    fs_delete(filename);
    return 1;
}

void fs_copy(char* source, char* dest) {
    static uint8_t raw_buffer[8192];
    int64_t bytes_read = exfat_read_file(source, raw_buffer, sizeof(raw_buffer) - 1);
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

int fs_mount(void) {
    return exfat_mount();
}