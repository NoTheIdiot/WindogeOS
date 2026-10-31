#include <system.h>
#include <stddef.h>
#include <dogeio.h>
#include <stdint.h>

#define ELF_MAGIC 0x464C457F

void system_exec(char* path) {
    uint8_t buffer[4];
    fs_read_raw(path, buffer, 4);

    if (*(uint32_t*)buffer == ELF_MAGIC) {
        system_run_elf_impl(path, MAX_FLAT_BINARY_SIZE);
    } else {
        system_run_bin(path, MAX_FLAT_BINARY_SIZE);
    }
}
