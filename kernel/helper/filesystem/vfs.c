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