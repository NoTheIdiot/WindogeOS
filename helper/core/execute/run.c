#include <system.h>
#include <stddef.h>
#include <dogeio.h>
#include <stdint.h>

void system_exec(char* path) {
    system_exec_args(path, 0, NULL);
}

void system_exec_args(char *path, int argc, char **argv) {
    uint64_t file_size;
    bool is_dir;
    if (path == NULL || path[0] == '\0' || argc < 0 || argc > 32 ||
        (argc > 0 && argv == NULL)) {
        dogeio_println("[Error] Invalid executable path or arguments.");
        return;
    }
    if (fs_get_info(path, &file_size, &is_dir) != 0 || is_dir ||
        file_size == 0 || file_size > MAX_FLAT_BINARY_SIZE) {
        dogeio_println("[Error] Invalid or unsupported executable file.");
        return;
    }

    uint8_t buffer[4] = {0};
    int read_bytes = fs_read_raw(path, buffer, sizeof(buffer));
    if (read_bytes < 0) {
        dogeio_println("[Error] Failed to read executable file.");
        return;
    }

    if (read_bytes == (int)sizeof(buffer) && buffer[0] == 0x7F &&
        buffer[1] == 'E' && buffer[2] == 'L' && buffer[3] == 'F') {
        system_run_elf_args(path, file_size, argc, argv);
    } else {
        system_run_bin_args(path, (int)file_size, argc, argv);
    }
}
