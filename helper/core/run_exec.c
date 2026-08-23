#include <dogeio.h>
#include <stdint.h>
#include <string.h>
#include <basicutil.h>

#define MAX_APP_SIZE (2 * 1024 * 1024)

typedef int (*app_entry_t)(int argc, char **argv);

static uint8_t app_exec_buffer[MAX_APP_SIZE] __attribute__((aligned(4096)));

int exec_flat_binary(const char *filename, int argc, char **argv) {
    if (!fs_exists((char *)filename)) {
        dogeio_text_println("Error: Binary file not found.");
        return -1;
    }

    memset(app_exec_buffer, 0, sizeof(app_exec_buffer));

    int bytes_read = fs_read((char *)filename, (char *)app_exec_buffer, MAX_APP_SIZE);
    if (bytes_read <= 0) {
        log("Failed to read file");
        return -2;
    }

    app_entry_t app_entry = (app_entry_t)app_exec_buffer;
    return app_entry(argc, argv);
}