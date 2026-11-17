#include <system.h>
#include <stdint.h>
#include <dogeio.h>

void run(char* file) {
    if (!fs_exists(file)) {
        return;
    }
    run_elf(file, fs_get_size(file), 0, NULL);
}

void run_args(char* file, int argc, char** argv) {
    if (!fs_exists(file)) {
        return;
    }
    run_elf(file, fs_get_size(file), argc, argv);
}