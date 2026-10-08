#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>

int util_hexdump(char* filename) {
    if (!fs_exists(filename)) {
        return -1;
    }

    uint8_t buffer[1028] = {0};
    int bytes_read = fs_read_raw(filename, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        return -1;
    }

    for (int i = 0; i < bytes_read; i++) {
        if (i % 16 == 0) {
            dogeio_print_hex16((uint16_t)i);
            dogeio_text_print(": ");
        }
        dogeio_print_hex8(buffer[i]);
        dogeio_text_print(" ");
        if (i % 16 == 15 || i + 1 == bytes_read) {
            dogeio_text_println("");
        }
    }

    return 0;
}
