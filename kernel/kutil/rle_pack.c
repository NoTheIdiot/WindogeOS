#include <dogeio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define CHUNK_SIZE 64
#define RLE_BUF_SIZE 64

void kernel_rle_decompress(char *src_path, uint64_t src_size, char *dst_path) {
    if (src_size < 4) {
        dogeio_text_println("[RLE] Error: RLE file too small.");
        return;
    }

    uint32_t original_size = 0;
    fs_read_raw_at(src_path, (uint8_t *)&original_size, 0, 4);

    if (original_size == 0) {
        dogeio_text_println("[RLE] Error: Compressed header reads 0 bytes.");
        return;
    }

    fs_create(dst_path);

    uint64_t src_idx = 4;
    uint64_t dst_idx = 0;
    uint8_t local_buffer[RLE_BUF_SIZE];
    uint32_t local_buf_idx = 0;

    while (src_idx < src_size && dst_idx < (uint64_t)original_size) {
        uint8_t control_buf = 0;
        fs_read_raw_at(src_path, &control_buf, src_idx, 1);
        src_idx++;
        int8_t control = (int8_t)control_buf;

        if (control >= 0) {
            uint64_t count = (uint64_t)control + 1;
            
            while (count > 0 && src_idx < src_size && dst_idx < (uint64_t)original_size) {
                uint8_t chunk[CHUNK_SIZE];
                uint64_t batch = count > CHUNK_SIZE ? CHUNK_SIZE : count;
                
                if (src_idx + batch > src_size) {
                    batch = src_size - src_idx;
                }

                fs_read_raw_at(src_path, chunk, src_idx, (uint32_t)batch);
                src_idx += batch;

                for (uint64_t i = 0; i < batch && dst_idx < (uint64_t)original_size; i++) {
                    local_buffer[local_buf_idx++] = chunk[i];
                    dst_idx++;

                    if (local_buf_idx >= RLE_BUF_SIZE) {
                        fs_append_data(dst_path, local_buffer, local_buf_idx);
                        local_buf_idx = 0;
                    }
                }
                count -= batch;
            }
        } else {
            uint64_t count = (uint64_t)(-control) + 1;
            uint8_t repeat_value = 0;

            if (src_idx < src_size) {
                fs_read_raw_at(src_path, &repeat_value, src_idx, 1);
                src_idx++;
                
                while (count > 0 && dst_idx < (uint64_t)original_size) {
                    uint64_t batch = count > CHUNK_SIZE ? CHUNK_SIZE : count;

                    for (uint64_t i = 0; i < batch && dst_idx < (uint64_t)original_size; i++) {
                        local_buffer[local_buf_idx++] = repeat_value;
                        dst_idx++;

                        if (local_buf_idx >= RLE_BUF_SIZE) {
                            fs_append_data(dst_path, local_buffer, local_buf_idx);
                            local_buf_idx = 0;
                        }
                    }
                    count -= batch;
                }
            }
        }
    }

    if (local_buf_idx > 0) {
        fs_append_data(dst_path, local_buffer, local_buf_idx);
    }
    
    dogeio_text_println("[RLE] Decompression completed successfully.");
}

void kernel_rle_compress(char *src_path, uint64_t src_size, char *dst_path) {
    fs_create(dst_path);
    uint32_t header_size = (uint32_t)src_size;
    fs_append_data(dst_path, (const uint8_t *)&header_size, 4);

    uint64_t src_idx = 0;
    uint8_t read_window[CHUNK_SIZE];

    while (src_idx < src_size) {
        uint64_t run_len = 1;
        uint8_t current_val = 0;
        fs_read_raw_at(src_path, &current_val, src_idx, 1);

        while (src_idx + run_len < src_size && run_len < 128) {
            uint8_t next_val = 0;
            fs_read_raw_at(src_path, &next_val, src_idx + run_len, 1);
            if (next_val != current_val) break;
            run_len++;
        }

        if (run_len >= 2) {
            int8_t ctrl = (int8_t)(-(int64_t)(run_len - 1));
            uint8_t payload[2] = {(uint8_t)ctrl, current_val};
            fs_append_data(dst_path, payload, 2);
            src_idx += run_len;
        } else {
            uint64_t lit_len = 0;
            while (src_idx + lit_len < src_size && lit_len < 128) {
                if (src_idx + lit_len + 1 < src_size) {
                    uint8_t v1 = 0, v2 = 0;
                    fs_read_raw_at(src_path, &v1, src_idx + lit_len, 1);
                    fs_read_raw_at(src_path, &v2, src_idx + lit_len + 1, 1);
                    if (v1 == v2) break; 
                }
                lit_len++;
            }

            if (lit_len > 0) {
                int8_t ctrl = (int8_t)(lit_len - 1);
                fs_append_data(dst_path, (uint8_t *)&ctrl, 1);

                uint64_t written = 0;
                while (written < lit_len) {
                    uint64_t batch = (lit_len - written) > CHUNK_SIZE ? CHUNK_SIZE : (lit_len - written);
                    fs_read_raw_at(src_path, read_window, src_idx + written, (uint32_t)batch);
                    fs_append_data(dst_path, read_window, (uint32_t)batch);
                    written += batch;
                }
                src_idx += lit_len;
            }
        }
    }
    dogeio_text_println("[RLE] Compression completed successfully.");
}
