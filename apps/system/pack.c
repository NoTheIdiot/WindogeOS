#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/stddef.h"
#include "../library/stdint.h"

#if defined(__GNUC__) || defined(__clang__)
    #define UNUSED __attribute__((unused))
#else
    #define UNUSED
#endif

UNUSED static inline void decompress(char* file, uint64_t size, char* dest) {
    if (size < 4) {
        println("RLE Error: Compressed RLE file too small. Much Error.");
        return;
    }

    if (file_exists(dest)) {
        println("RLE Error: Dest file already exists. Why?");
        return;
    }

    if (!file_exists(file)) {
        println("RLE Error: Input file doesn't exist.");
        return;
    }

    uint64_t original_size = 0;
    read_file_raw_at(file, (uint8_t *)&original_size, 0, 4);
}
/* clang shut up
UNUSED static inline void compress(char* file, uint64_t size, char* dest) {
    if (file_exists(dest)) {
        println("RLE Error: Dest file already exists. Not Wow.");
        return;
    }
}*/

void _start(void) {
    sys_exit(0);
}