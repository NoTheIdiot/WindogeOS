#include <dogeio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define LZ4_WINDOW_SIZE 4096U
#define LZ4_READ_AHEAD_SIZE 4096U
#define LZ4_OUTPUT_SIZE 65536U
#define LZ4_BLOCK_BUFFER_SIZE 8192U
#define LZ4_HASH_SIZE 4096U

static uint8_t lz4_output_buffer[LZ4_OUTPUT_SIZE];
static uint8_t lz4_block_buffer[LZ4_BLOCK_BUFFER_SIZE];
static uint8_t lz4_history_buffer[LZ4_WINDOW_SIZE];
static uint32_t lz4_hash_table[LZ4_HASH_SIZE];

typedef struct {
    char *path;
    uint64_t file_size;
    uint64_t start;
    uint32_t length;
    uint8_t data[LZ4_READ_AHEAD_SIZE];
} lz4_read_buffer_t;

typedef struct {
    char *path;
    uint8_t *data;
    uint32_t length;
    uint32_t capacity;
} lz4_write_buffer_t;

typedef struct {
    char *path;
    uint8_t *history;
    uint8_t *data;
    uint32_t length;
    uint32_t produced;
} lz4_decode_output_t;

static int lz4_read_at(lz4_read_buffer_t *buffer, uint64_t offset,
                       uint8_t *destination, uint32_t length) {
    while (length > 0) {
        if (offset >= buffer->file_size) return -1;
        if (offset < buffer->start ||
            offset - buffer->start >= buffer->length) {
            uint64_t requested = buffer->file_size - offset;
            if (requested > LZ4_READ_AHEAD_SIZE) {
                requested = LZ4_READ_AHEAD_SIZE;
            }
            int bytes_read = fs_read_raw_at(buffer->path, buffer->data,
                                            offset, requested);
            if (bytes_read <= 0) return -1;
            buffer->start = offset;
            buffer->length = (uint32_t)bytes_read;
        }

        uint32_t buffer_offset = (uint32_t)(offset - buffer->start);
        uint32_t available = buffer->length - buffer_offset;
        uint32_t batch = length < available ? length : available;
        memcpy(destination, buffer->data + buffer_offset, batch);
        destination += batch;
        offset += batch;
        length -= batch;
    }
    return 0;
}

static int lz4_write_flush(lz4_write_buffer_t *buffer) {
    if (buffer->length == 0) return 0;
    if (fs_append_data(buffer->path, buffer->data, buffer->length) != 0) {
        return -1;
    }
    buffer->length = 0;
    return 0;
}

static int lz4_write(lz4_write_buffer_t *buffer, const uint8_t *data,
                     uint32_t length) {
    while (length > 0) {
        uint32_t available = buffer->capacity - buffer->length;
        uint32_t batch = length < available ? length : available;
        memcpy(buffer->data + buffer->length, data, batch);
        buffer->length += batch;
        data += batch;
        length -= batch;

        if (buffer->length == buffer->capacity) {
            if (buffer->path == NULL || lz4_write_flush(buffer) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int lz4_write_length(lz4_write_buffer_t *output, uint32_t length) {
    while (length >= 255U) {
        uint8_t extension = 255U;
        if (lz4_write(output, &extension, 1) != 0) return -1;
        length -= 255U;
    }
    uint8_t extension = (uint8_t)length;
    return lz4_write(output, &extension, 1);
}

static int lz4_write_sequence(lz4_write_buffer_t *output,
                              const uint8_t *literals,
                              uint32_t literal_length,
                              uint32_t match_length,
                              uint32_t match_offset) {
    uint32_t encoded_match_length =
        match_length > 0 ? match_length - 4U : 0U;
    uint8_t token = (uint8_t)(
        ((literal_length < 15U ? literal_length : 15U) << 4) |
        (encoded_match_length < 15U ? encoded_match_length : 15U));

    if (lz4_write(output, &token, 1) != 0) return -1;
    if (literal_length >= 15U &&
        lz4_write_length(output, literal_length - 15U) != 0) {
        return -1;
    }
    if (lz4_write(output, literals, literal_length) != 0) return -1;

    if (match_length == 0) return 0;

    uint8_t offset_bytes[2] = {
        (uint8_t)(match_offset & 0xFFU),
        (uint8_t)((match_offset >> 8) & 0xFFU)
    };
    if (lz4_write(output, offset_bytes, sizeof(offset_bytes)) != 0) {
        return -1;
    }
    if (encoded_match_length >= 15U &&
        lz4_write_length(output, encoded_match_length - 15U) != 0) {
        return -1;
    }
    return 0;
}

static uint32_t lz4_hash(const uint8_t *data, uint32_t offset) {
    uint32_t value = (uint32_t)data[offset] |
                     ((uint32_t)data[offset + 1U] << 8) |
                     ((uint32_t)data[offset + 2U] << 16) |
                     ((uint32_t)data[offset + 3U] << 24);
    return (value * 2654435761U) >> 20;
}

static int lz4_compress_block(lz4_write_buffer_t *output,
                              const uint8_t *source, uint32_t source_size) {
    memset(lz4_hash_table, 0xFF, sizeof(lz4_hash_table));
    uint32_t anchor = 0;
    uint32_t index = 0;

    while (source_size >= 12U && index <= source_size - 12U) {
        uint32_t hash = lz4_hash(source, index);
        uint32_t candidate = lz4_hash_table[hash];
        lz4_hash_table[hash] = index;

        if (candidate == UINT32_MAX ||
            index - candidate > LZ4_WINDOW_SIZE ||
            source[candidate] != source[index] ||
            source[candidate + 1U] != source[index + 1U] ||
            source[candidate + 2U] != source[index + 2U] ||
            source[candidate + 3U] != source[index + 3U]) {
            index++;
            continue;
        }

        uint32_t match_length = 4U;
        uint32_t maximum_match = source_size - 5U - index;
        while (match_length < maximum_match &&
               source[candidate + match_length] ==
                   source[index + match_length]) {
            match_length++;
        }

        if (lz4_write_sequence(output, source + anchor, index - anchor,
                               match_length, index - candidate) != 0) {
            return -1;
        }

        uint32_t match_end = index + match_length;
        for (uint32_t pos = index + 1U;
             pos < match_end && pos <= source_size - 4U; pos++) {
            lz4_hash_table[lz4_hash(source, pos)] = pos;
        }
        index = match_end;
        anchor = index;
    }

    if (source_size > 0 &&
        lz4_write_sequence(output, source + anchor, source_size - anchor,
                           0, 0) != 0) {
        return -1;
    }
    return 0;
}

static int lz4_output_byte(lz4_decode_output_t *output, uint8_t value) {
    output->history[output->produced & (LZ4_WINDOW_SIZE - 1U)] = value;
    output->data[output->length++] = value;
    output->produced++;

    if (output->length == LZ4_OUTPUT_SIZE) {
        if (fs_append_data(output->path, output->data, output->length) != 0) {
            return -1;
        }
        output->length = 0;
    }
    return 0;
}

static int lz4_read_length(lz4_read_buffer_t *input, uint64_t *src_idx,
                           uint64_t limit, uint64_t *length) {
    uint8_t extension = 0;
    do {
        if (*src_idx >= limit ||
            lz4_read_at(input, *src_idx, &extension, 1) != 0) {
            return -1;
        }
        (*src_idx)++;
        if (UINT64_MAX - *length < extension) return -1;
        *length += extension;
    } while (extension == 255U);
    return 0;
}

int kernel_lz4_decompress(char *src_path, uint64_t src_size,
                          char *dst_path) {
    if (src_size < 4U) {
        dogeio_println("[LZ4] Error: Compressed file is too small.");
        return -1;
    }

    lz4_read_buffer_t input = {
        .path = src_path,
        .file_size = src_size,
        .start = UINT64_MAX,
        .length = 0
    };
    uint8_t header[4];
    if (lz4_read_at(&input, 0, header, sizeof(header)) != 0) {
        dogeio_println("[LZ4] Error: Unable to read compressed header.");
        return -1;
    }

    uint32_t original_size = (uint32_t)header[0] |
                             ((uint32_t)header[1] << 8) |
                             ((uint32_t)header[2] << 16) |
                             ((uint32_t)header[3] << 24);
    uint8_t marker[4];
    int framed = src_size >= 8U &&
                 lz4_read_at(&input, 4, marker, sizeof(marker)) == 0 &&
                 marker[0] == 'L' && marker[1] == 'Z' &&
                 marker[2] == '4' && marker[3] == 'C';
    if (original_size == 0U) {
        uint64_t expected_size = framed ? 8U : sizeof(header);
        if (src_size != expected_size || fs_create(dst_path) != 0) {
            dogeio_println("[LZ4] Error: Invalid empty archive or destination.");
            return -1;
        }
        dogeio_println("[LZ4] Decompression completed successfully.");
        return 0;
    }
    if (fs_create(dst_path) != 0) {
        dogeio_println("[LZ4] Error: Unable to create decompressed file.");
        return -1;
    }

    lz4_decode_output_t output = {
        .path = dst_path,
        .history = lz4_history_buffer,
        .data = lz4_output_buffer,
        .length = 0,
        .produced = 0
    };
    uint64_t src_idx = framed ? 8U : 4U;
    uint8_t literal_buffer[512];
    int failed = 0;

    while (!failed && output.produced < original_size) {
        uint64_t block_end = src_size;
        uint32_t block_start = output.produced;
        uint32_t block_target = original_size;
        if (framed) {
            uint8_t block_header[8];
            if (src_size - src_idx < sizeof(block_header) ||
                lz4_read_at(&input, src_idx, block_header,
                            sizeof(block_header)) != 0) {
                failed = 1;
                break;
            }
            src_idx += sizeof(block_header);
            uint32_t block_original_size =
                (uint32_t)block_header[0] |
                ((uint32_t)block_header[1] << 8) |
                ((uint32_t)block_header[2] << 16) |
                ((uint32_t)block_header[3] << 24);
            uint32_t block_compressed_size =
                (uint32_t)block_header[4] |
                ((uint32_t)block_header[5] << 8) |
                ((uint32_t)block_header[6] << 16) |
                ((uint32_t)block_header[7] << 24);
            if (block_original_size == 0U || block_compressed_size == 0U ||
                block_original_size > original_size - output.produced ||
                block_compressed_size > src_size - src_idx) {
                failed = 1;
                break;
            }
            block_target = output.produced + block_original_size;
            block_end = src_idx + block_compressed_size;
        }

        while (!failed && src_idx < block_end &&
               output.produced < block_target) {
            uint8_t token = 0;
            if (lz4_read_at(&input, src_idx, &token, 1) != 0) {
                failed = 1;
                break;
            }
            src_idx++;

            uint64_t literal_length = token >> 4;
            if (literal_length == 15U &&
                lz4_read_length(&input, &src_idx, block_end,
                                &literal_length) != 0) {
                failed = 1;
                break;
            }
            if (literal_length > (uint64_t)block_target - output.produced ||
                literal_length > block_end - src_idx) {
                failed = 1;
                break;
            }

            uint64_t literals_remaining = literal_length;
            while (literals_remaining > 0) {
                uint32_t batch = literals_remaining > sizeof(literal_buffer)
                    ? (uint32_t)sizeof(literal_buffer)
                    : (uint32_t)literals_remaining;
                if (lz4_read_at(&input, src_idx, literal_buffer, batch) != 0) {
                    failed = 1;
                    break;
                }
                src_idx += batch;
                literals_remaining -= batch;

                for (uint32_t i = 0; i < batch; i++) {
                    if (lz4_output_byte(&output, literal_buffer[i]) != 0) {
                        failed = 1;
                        break;
                    }
                }
                if (failed) break;
            }
            if (failed) break;

            if (output.produced == block_target) {
                if (src_idx != block_end) failed = 1;
                break;
            }

            uint8_t offset_bytes[2];
            if (block_end - src_idx < sizeof(offset_bytes) ||
                lz4_read_at(&input, src_idx, offset_bytes,
                            sizeof(offset_bytes)) != 0) {
                failed = 1;
                break;
            }
            src_idx += sizeof(offset_bytes);
            uint32_t match_offset = (uint32_t)offset_bytes[0] |
                                    ((uint32_t)offset_bytes[1] << 8);
            if (match_offset == 0 || match_offset > LZ4_WINDOW_SIZE ||
                match_offset > output.produced - block_start) {
                failed = 1;
                break;
            }

            uint64_t match_length = (uint64_t)(token & 0x0FU) + 4U;
            if ((token & 0x0FU) == 15U &&
                lz4_read_length(&input, &src_idx, block_end,
                                &match_length) != 0) {
                failed = 1;
                break;
            }
            if (match_length > (uint64_t)block_target - output.produced) {
                failed = 1;
                break;
            }

            for (uint64_t i = 0; i < match_length; i++) {
                uint32_t history_idx =
                    (output.produced - match_offset) &
                    (LZ4_WINDOW_SIZE - 1U);
                if (lz4_output_byte(&output,
                                    output.history[history_idx]) != 0) {
                    failed = 1;
                    break;
                }
            }
            if (failed) break;
        }

        if (output.produced != block_target || src_idx != block_end) {
            failed = 1;
        }
        if (!framed) break;
    }

    if (output.produced != original_size || src_idx != src_size) failed = 1;
    if (!failed && output.length > 0 &&
        fs_append_data(dst_path, output.data, output.length) != 0) {
        failed = 1;
    }
    if (failed) {
        fs_delete(dst_path);
        dogeio_println("[LZ4] Error: Invalid or unreadable compressed data.");
        return -1;
    } else {
        dogeio_println("[LZ4] Decompression completed successfully.");
    }
    return 0;
}

int kernel_lz4_compress(char *src_path, uint64_t src_size,
                    char *dst_path) {
    if (src_size > UINT32_MAX) {
        dogeio_println("[LZ4] Error: Source file is too large to compress.");
        return -1;
    }

    uint32_t source_size = (uint32_t)src_size;
    uint8_t source_buffer[LZ4_READ_AHEAD_SIZE];

    lz4_write_buffer_t output = {
        .path = dst_path,
        .data = lz4_output_buffer,
        .length = 0,
        .capacity = LZ4_OUTPUT_SIZE
    };
    if (fs_create(dst_path) != 0) {
        dogeio_println("[LZ4] Error: Unable to create compressed file.");
        return -1;
    }
    uint8_t header[8] = {
        (uint8_t)(source_size & 0xFFU),
        (uint8_t)((source_size >> 8) & 0xFFU),
        (uint8_t)((source_size >> 16) & 0xFFU),
        (uint8_t)((source_size >> 24) & 0xFFU),
        'L', 'Z', '4', 'C'
    };
    int failed = lz4_write(&output, header, sizeof(header)) != 0;
    uint32_t offset = 0;
    while (!failed && offset < source_size) {
        uint32_t requested = source_size - offset;
        if (requested > sizeof(source_buffer)) {
            requested = sizeof(source_buffer);
        }
        int bytes_read = fs_read_raw_at(src_path, source_buffer, offset,
                                        requested);
        if (bytes_read <= 0 || (uint32_t)bytes_read != requested) {
            failed = 1;
            break;
        }
        lz4_write_buffer_t block_output = {
            .path = NULL,
            .data = lz4_block_buffer,
            .length = 0,
            .capacity = LZ4_BLOCK_BUFFER_SIZE
        };
        if (lz4_compress_block(&block_output, source_buffer, requested) != 0 ||
            block_output.length == 0U ||
            block_output.length >= LZ4_BLOCK_BUFFER_SIZE) {
            failed = 1;
            break;
        }
        uint8_t block_header[8] = {
            (uint8_t)(requested & 0xFFU),
            (uint8_t)((requested >> 8) & 0xFFU),
            (uint8_t)((requested >> 16) & 0xFFU),
            (uint8_t)((requested >> 24) & 0xFFU),
            (uint8_t)(block_output.length & 0xFFU),
            (uint8_t)((block_output.length >> 8) & 0xFFU),
            (uint8_t)((block_output.length >> 16) & 0xFFU),
            (uint8_t)((block_output.length >> 24) & 0xFFU)
        };
        if (lz4_write(&output, block_header, sizeof(block_header)) != 0 ||
            lz4_write(&output, lz4_block_buffer, block_output.length) != 0) {
            failed = 1;
            break;
        }
        offset += requested;
    }
    if (!failed && lz4_write_flush(&output) != 0) failed = 1;

    if (failed) {
        fs_delete(dst_path);
        dogeio_println("[LZ4] Error: Unable to read source or write archive.");
        return -1;
    } else {
        dogeio_println("[LZ4] Compression completed successfully.");
    }
    return 0;
}
