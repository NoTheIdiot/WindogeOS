#include "../dogec/stdio.h"
#include "../dogec/dogeio.h"
#include "../dogec/stdint.h"
#include "../dogec/stddef.h"
#include "../dogec/string.h"

#define HEXDUMP_BYTES_PER_LINE 16

static const char hex_digits[] = "0123456789abcdef";

static void clear_bytes(uint8_t *buffer, size_t capacity) {
    for (size_t i = 0; i < capacity; i++) {
        buffer[i] = 0;
    }
}

static int text_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static void append_hex_byte(char *line, size_t *position, uint8_t value) {
    line[(*position)++] = hex_digits[value >> 4];
    line[(*position)++] = hex_digits[value & 0x0f];
}

static void append_hex_u64(char *line, size_t *position, uint64_t value) {
    for (int shift = 60; shift >= 0; shift -= 4) {
        line[(*position)++] = hex_digits[(value >> (uint32_t)shift) & 0x0f];
    }
}

static int dump_file(const char *path) {
    fs_t file_stat;
    memset(&file_stat, 0, sizeof(file_stat));
    if (fs_stat(path, &file_stat) != 0) {
        printf("hexdump: unable to access %s\n", path);
        return 1;
    }
    if (file_stat.is_dir != 0) {
        puts("hexdump: path is a directory");
        return 1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("hexdump: unable to open %s\n", path);
        return 1;
    }

    printf("Hex dump of %s (%llu bytes)\n",
           path,
           (unsigned long long)file_stat.size);

    uint64_t offset = 0;
    while (offset < file_stat.size) {
        uint8_t bytes[HEXDUMP_BYTES_PER_LINE];
        clear_bytes(bytes, sizeof(bytes));

        uint64_t amount = file_stat.size - offset;
        if (amount > sizeof(bytes)) {
            amount = sizeof(bytes);
        }

        if (fseek(fp, (long)offset, SEEK_SET) != 0) {
            puts("hexdump: unable to seek in file");
            fclose(fp);
            return 1;
        }

        size_t bytes_read = fread(bytes, 1, (size_t)amount, fp);
        if ((uint64_t)bytes_read != amount) {
            puts("hexdump: unable to read the complete file");
            fclose(fp);
            return 1;
        }

        char line[96];
        size_t position = 0;
        append_hex_u64(line, &position, offset);
        line[position++] = ' ';
        line[position++] = ' ';

        for (uint64_t i = 0; i < HEXDUMP_BYTES_PER_LINE; i++) {
            if (i < amount) {
                append_hex_byte(line, &position, bytes[i]);
            } else {
                line[position++] = ' ';
                line[position++] = ' ';
            }
            line[position++] = ' ';
        }

        line[position++] = '|';
        for (uint64_t i = 0; i < amount; i++) {
            uint8_t value = bytes[i];
            line[position++] =
                value >= 32 && value <= 126 ? (char)value : '.';
        }
        line[position++] = '|';
        line[position] = '\0';
        puts(line);

        offset += amount;
    }

    fclose(fp);
    return 0;
}

void _start(int argc, char **argv) {
    if (argc >= 2 && argv != NULL && argv[1] != NULL) {
        if (text_equal(argv[1], "--help")) {
            puts("Usage: hexdump <file>");
            exit();
        }
        if (text_equal(argv[1], "--version")) {
            puts("WindogeOS hexdump 1.0");
            exit();
        }
    }

    if (argc < 2 || argv == NULL || argv[1] == NULL ||
        argv[1][0] == '\0') {
        puts("Usage: hexdump <file>");
        exit();
        return;
    }

    (void)dump_file(argv[1]);
    exit();
}