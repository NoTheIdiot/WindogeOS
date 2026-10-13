#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/stdint.h"
#include "../library/stddef.h"

void rle_signaling_decompress(const uint8_t *src, int src_size, uint8_t *dst) {
    int src_idx = 0;
    int dst_idx = 0;
    while (src_idx < src_size) {
        int8_t control = (int8_t)src[src_idx++]; 
        if (control >= 0) {
            int count = control + 1;
            for (int i = 0; i < count; i++) {
                dst[dst_idx++] = src[src_idx++];
            }
        } else {
            int count = (-control) + 1;
            uint8_t value = src[src_idx++];
            for (int i = 0; i < count; i++) {
                dst[dst_idx++] = value;
            }
        }
    }
}

int rle_signaling_compress(const uint8_t *src, int src_size, uint8_t *dst) {
    int src_idx = 0;
    int dst_idx = 0;
    while (src_idx < src_size) {
        int run_len = 1;
        while (src_idx + run_len < src_size && 
               src[src_idx + run_len] == src[src_idx] && run_len < 128) {
            run_len++;
        }
        if (run_len > 1) {
            dst[dst_idx++] = (uint8_t)(-(run_len - 1));
            dst[dst_idx++] = src[src_idx];
            src_idx += run_len;
        } else {
            int lit_len = 0;
            while (src_idx + lit_len < src_size && lit_len < 128) {
                if (src_idx + lit_len + 1 < src_size && 
                    src[src_idx + lit_len] == src[src_idx + lit_len + 1]) {
                    break;
                }
                lit_len++;
            }
            dst[dst_idx++] = (uint8_t)(lit_len - 1);
            for (int i = 0; i < lit_len; i++) {
                dst[dst_idx++] = src[src_idx++];
            }
        }
    }
    return dst_idx;
}

void _start(int argc, char **argv) {
    if (argc == 1 && argv != NULL && argv[0] != NULL) {
        if (str_strcmp(argv[0], "--help") == 0) {\
            println("RLEPACK (RLE packer)");
            println("Simply compress and decompress files.");
            println("Usage: rlepack <file>");
            sys_exit(0);
        }
        if (str_strcmp(argv[0], "--version") == 0) {
            println("RLEPACK");
            sys_exit(0);
        }
    }

    if (argc != 1 || argv == NULL || argv[0] == NULL ||
        argv[0][0] == '\0') {
        println("Usage: rlepack <file>");
        sys_exit(2);
        return;
    }

    sys_exit((uint64_t)1);
}
