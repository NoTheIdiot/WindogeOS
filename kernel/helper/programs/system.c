#include <system.h>
#include <stdint.h>
#include <dogeio.h>

void run(char* file) {
    if (!fs_exists(file)) {
        return;
    }
    run_elf(file, fs_get_size(file));
}