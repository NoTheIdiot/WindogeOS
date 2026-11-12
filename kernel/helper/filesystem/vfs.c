#include <dogeio.h>
#include <system.h>
#include <string.h>
#include <bool.h>

mount_t exfat_vol;

int fs_mount(void) {
    static uint8_t buffer[32768];
    int status = mount(&exfat_vol, buffer);
    if (status == 0) {
        memset(buffer, 0, exfat_vol.bytes_per_cluster);
    }
    return status;
}

int fs_create(char* filename) {
    return create_file(&exfat_vol, filename);
}

int fs_write_force(char* filename, char* data, uint64_t size) {
    return write_file(&exfat_vol, filename, (uint8_t*)data, size);
}

int fs_read(char* filename, char* data, uint64_t size) {
    uint64_t buffer;
    read_file(&exfat_vol, filename, (uint8_t*)data, size, &buffer);
    return buffer;
}

bool fs_exists(char *filename) {
    char dummy;
    return fs_read(filename, &dummy, 0) == 0;
}

int fs_write(char* filename, char* data, uint64_t size) {
    while (size > 0 && data[size - 1] == '\0') {
        size--;
    }

    int status = append_file(&exfat_vol, filename, (uint8_t*)data, size);
    if (status != 0) {
        return status;
    }

    char nl = '\n';
    return append_file(&exfat_vol, filename, (uint8_t*)&nl, 1);
}

int fs_list(fs_t* out, int max_entries) {
    return list_dir(&exfat_vol, out, max_entries);
}

int fs_delete(char* filename) {
    return delete_file(&exfat_vol, filename);
}

int fs_read_raw(char *filename, uint8_t *buffer, uint64_t size) {
    uint64_t bytes_read = 0;

    if (read_file(&exfat_vol, filename, buffer, size, &bytes_read) != 0) {
        return -1;
    }

    return (int)bytes_read;
}

int fs_write_raw(char* filename, uint8_t* buffer, uint64_t size) {
    return write_file(&exfat_vol, filename, buffer, size);
}

int fs_read_raw_at(char *filename, uint8_t *buffer, uint64_t offset, uint64_t size) {
    static uint8_t temp[16 * 1024 * 1024];

    uint64_t bytes_read = 0;

    if (read_file(&exfat_vol, filename, temp, sizeof(temp), &bytes_read) != 0){
        return -1;
    }

    if (offset >= bytes_read) {
        return 0;
    }

    if (offset + size > bytes_read) {
        size = bytes_read - offset;
    }

    memcpy(buffer, temp + offset, size);

    return (int)size;
}