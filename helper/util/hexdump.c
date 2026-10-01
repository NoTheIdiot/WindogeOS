#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>

int util_hexdump(char* filename) {
    if (!fs_exists(filename)) {
        return -1;
    }

    uint8_t buffer[1028];
    fs_read_raw(filename, buffer, 1028);
    
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 16; j++) {
            int index = (i * 16) + j;
            dogeio_print_hex8(buffer[index]);
            dogeio_text_print(" ");
        }
        dogeio_text_println("");
    }

    return 0;
}
