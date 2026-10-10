#include "../library/dogeio.h"
#include "../library/stdint.h"
#include "../library/stddef.h"
#include "../library/string.h"

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
    dogec_stat_t file_stat;
    file_stat.size = 0;
    file_stat.is_dir = 0;
    file_stat.exists = 0;
    if ((int64_t)stat(path, &file_stat) < 0) {
        print("hexdump: unable to access ");
        println(path);
        return 1;
    }
    if (file_stat.is_dir != 0) {
        println("hexdump: path is a directory");
        return 1;
    }

    print("Hex dump of ");
    print(path);
    print(" (");
    char size_text[21];
    str_u64toa(file_stat.size, size_text);
    print(size_text);
    println(" bytes)");

    uint64_t offset = 0;
    while (offset < file_stat.size) {
        uint8_t bytes[HEXDUMP_BYTES_PER_LINE];
        clear_bytes(bytes, sizeof(bytes));
        uint64_t amount = file_stat.size - offset;
        if (amount > sizeof(bytes)) {
            amount = sizeof(bytes);
        }

        int64_t bytes_read =
            (int64_t)read_file_at(path, bytes, offset, amount);
        if (bytes_read < 0 || (uint64_t)bytes_read != amount) {
            println("hexdump: unable to read the complete file");
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
        println(line);

        offset += amount;
    }
    return 0;
}

void _start(int argc, char **argv) {
    if (argc == 1 && argv != NULL && argv[0] != NULL) {
        if (text_equal(argv[0], "--help")) {
            println("Usage: hexdump <file>");
            sys_exit(0);
        }
        if (text_equal(argv[0], "--version")) {
            println("WindogeOS hexdump 1.0");
            sys_exit(0);
        }
    }

    if (argc != 1 || argv == NULL || argv[0] == NULL ||
        argv[0][0] == '\0') {
        println("Usage: hexdump <file>");
        sys_exit(2);
        return;
    }

    sys_exit((uint64_t)dump_file(argv[0]));
}