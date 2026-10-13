#include <dogeio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

void kernel_rle_decompress(char *src_path, uint64_t src_size, char *dst_path) {
    if (src_size < 4) {
        dogeio_text_println("[RLE] Error: RLE file too small.");
        return;
    }

    uint32_t original_size = 0;
    fs_read_raw_at(src_path, (uint8_t *)&original_size, 0, 4);

    fs_create(dst_path);

    uint64_t src_idx = 4;
    uint64_t dst_idx = 0;
    uint8_t control_buf = 0;
    uint8_t value_buf = 0;

    while (src_idx < src_size && dst_idx < (uint64_t)original_size) {
        fs_read_raw_at(src_path, &control_buf, src_idx, 1);
        src_idx++;
        int8_t control = (int8_t)control_buf;

        if (control >= 0) {
            uint64_t count = (uint64_t)control + 1;
            for (uint64_t i = 0; i < count && src_idx < src_size && dst_idx < (uint64_t)original_size; i++) {
                fs_read_raw_at(src_path, &value_buf, src_idx, 1);
                src_idx++;
                fs_append_data(dst_path, &value_buf, 1);
                dst_idx++;
            }
        } else {
            uint64_t count = (uint64_t)(-control) + 1;
            if (src_idx < src_size) {
                fs_read_raw_at(src_path, &value_buf, src_idx, 1);
                src_idx++;
                for (uint64_t i = 0; i < count && dst_idx < (uint64_t)original_size; i++) {
                    fs_append_data(dst_path, &value_buf, 1);
                    dst_idx++;
                }
            }
        }
    }
    dogeio_text_println("[RLE] Decompression completed successfully.");
}

void kernel_rle_compress(char *src_path, uint64_t src_size, char *dst_path) {
    fs_create(dst_path);
    uint32_t header_size = (uint32_t)src_size;
    fs_append_data(dst_path, (const uint8_t *)&header_size, 4);

    uint64_t src_idx = 0;
    uint64_t dst_idx = 4;
    uint8_t current_val = 0;
    uint8_t next_val = 0;
    uint8_t out_ctrl = 0;

    while (src_idx < src_size) {
        fs_read_raw_at(src_path, &current_val, src_idx, 1);
        uint64_t run_len = 1;

        while (src_idx + run_len < src_size && run_len < 128) {
            fs_read_raw_at(src_path, &next_val, src_idx + run_len, 1);
            if (next_val != current_val) {
                break;
            }
            run_len++;
        }

        if (run_len > 1) {
            out_ctrl = (uint8_t)(-(int64_t)(run_len - 1));
            fs_append_data(dst_path, &out_ctrl, 1);
            fs_append_data(dst_path, &current_val, 1);
            dst_idx += 2;
            src_idx += run_len;
        } else {
            uint64_t lit_len = 0;
            while (src_idx + lit_len < src_size && lit_len < 128) {
                if (src_idx + lit_len + 1 < src_size) {
                    fs_read_raw_at(src_path, &current_val, src_idx + lit_len, 1);
                    fs_read_raw_at(src_path, &next_val, src_idx + lit_len + 1, 1);
                    if (current_val == next_val) {
                        break;
                    }
                }
                lit_len++;
            }

            out_ctrl = (uint8_t)(lit_len - 1);
            fs_append_data(dst_path, &out_ctrl, 1);
            dst_idx++;

            for (uint64_t i = 0; i < lit_len; i++) {
                fs_read_raw_at(src_path, &current_val, src_idx++, 1);
                fs_append_data(dst_path, &current_val, 1);
                dst_idx++;
            }
        }

        if (dst_idx > src_size + 4) {
            fs_delete(dst_path);
            dogeio_text_println("[RLE] Notice: File cannot be compressed efficiently. Aborting.");
            return;
        }
    }
    dogeio_text_println("[RLE] Compression completed successfully.");
}
