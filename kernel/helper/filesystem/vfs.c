#include <dogeio.h>
#include <system.h>
#include <string.h>

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

int fs_write(char* filename, char* data, uint64_t size) {
    return write_file(&exfat_vol, filename, (void*)data, size);
}

int fs_read(char* filename, char* data, uint64_t size) {
    uint64_t buffer;
    read_file(&exfat_vol, filename, (void*)data, size, &buffer);
    return buffer;
}