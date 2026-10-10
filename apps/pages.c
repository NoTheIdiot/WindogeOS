#include "../library/dogeio.h"
#include "../library/stdint.h"
#include "../library/stddef.h"
#include "../library/string.h"
#include "../library/bool.h"

#define PAGES_CONTENT_ROWS 46
#define PAGES_LINE_WIDTH 160
#define PAGES_READ_SIZE 512
#define PAGES_HISTORY_SIZE 64
#define PAGES_KEY_UP 0x101
#define PAGES_KEY_DOWN 0x102
#define PAGES_COLOR_WHITE 0xAAAAAAU
#define PAGES_COLOR_BRIGHT_CYAN 0x55FFFFU
#define PAGES_COLOR_BRIGHT_WHITE 0xFFFFFFU
#define PAGES_COLOR_BRIGHT_YELLOW 0xFFED29U

typedef struct {
    const char *path;
    uint64_t file_size;
    uint64_t buffer_start;
    size_t buffer_length;
    uint8_t buffer[PAGES_READ_SIZE];
} file_reader_t;

static uint64_t page_history[PAGES_HISTORY_SIZE];
static size_t page_history_count;

static int text_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static size_t append_text(char *destination, size_t position,
                          const char *text) {
    while (*text != '\0') {
        destination[position++] = *text++;
    }
    return position;
}

static size_t append_u64(char *destination, size_t position,
                         uint64_t value) {
    char digits[21];
    str_u64toa(value, digits);
    return append_text(destination, position, digits);
}

static int read_byte(file_reader_t *reader, uint64_t offset, uint8_t *value) {
    if (offset >= reader->file_size) {
        return 0;
    }

    if (offset < reader->buffer_start ||
        offset - reader->buffer_start >= reader->buffer_length) {
        uint64_t chunk_start =
            offset - (offset % (uint64_t)sizeof(reader->buffer));
        uint64_t amount = reader->file_size - chunk_start;
        if (amount > sizeof(reader->buffer)) {
            amount = sizeof(reader->buffer);
        }
        int64_t bytes_read = (int64_t)read_file_at(
            reader->path, reader->buffer, chunk_start, amount);
        if (bytes_read < 0 || (uint64_t)bytes_read != amount) {
            return -1;
        }
        reader->buffer_start = chunk_start;
        reader->buffer_length = (size_t)amount;
    }

    *value = reader->buffer[(size_t)(offset - reader->buffer_start)];
    return 1;
}

static int draw_page(file_reader_t *reader, uint64_t start,
                     uint64_t *next_start) {
    clear();
    print_at(reader->path, 0, 0, PAGES_COLOR_BRIGHT_CYAN);
    print_at("Use Up/Down or Space/B to page; G = start; Q = quit",
             0, 48, PAGES_COLOR_BRIGHT_WHITE);

    uint64_t offset = start;
    uint32_t row = 0;
    char line[PAGES_LINE_WIDTH + 1];

    while (row < PAGES_CONTENT_ROWS && offset < reader->file_size) {
        size_t column = 0;
        bool line_finished = false;
        while (offset < reader->file_size) {
            uint8_t value;
            int read_result = read_byte(reader, offset, &value);
            if (read_result < 0) {
                return -1;
            }
            if (read_result == 0) {
                break;
            }
            offset++;
            if (value == '\n') {
                line_finished = true;
                break;
            }
            if (value == '\r') {
                continue;
            }
            if (column < PAGES_LINE_WIDTH) {
                line[column++] =
                    value >= 32 && value <= 126 ? (char)value : '.';
            }
        }

        line[column] = '\0';
        (void)print_at(line, 0, row + 1, PAGES_COLOR_WHITE);
        row++;
        if (!line_finished && offset >= reader->file_size) {
            break;
        }
    }

    *next_start = offset;
    char status[80];
    size_t status_length = append_text(status, 0, "Offset: ");
    status_length = append_u64(status, status_length, start);
    status_length = append_text(status, status_length, " / ");
    status_length = append_u64(status, status_length, reader->file_size);
    status_length = append_text(status, status_length, " bytes");
    status[status_length] = '\0';
    print_at(status, 0, 47, PAGES_COLOR_BRIGHT_YELLOW);
    return 0;
}

static void remember_page(uint64_t start) {
    if (page_history_count == PAGES_HISTORY_SIZE) {
        for (size_t i = 1; i < PAGES_HISTORY_SIZE; i++) {
            page_history[i - 1] = page_history[i];
        }
        page_history_count--;
    }
    page_history[page_history_count++] = start;
}

static int view_file(const char *path) {
    dogec_stat_t file_stat;
    file_stat.size = 0;
    file_stat.is_dir = 0;
    file_stat.exists = 0;
    if ((int64_t)stat(path, &file_stat) < 0) {
        print("pages: unable to access ");
        println(path);
        return 1;
    }
    if (file_stat.is_dir != 0) {
        println("pages: path is a directory");
        return 1;
    }

    file_reader_t reader;
    reader.path = path;
    reader.file_size = file_stat.size;
    reader.buffer_start = 0;
    reader.buffer_length = 0;
    uint64_t page_start = 0;
    int status = 0;

    while (1) {
        uint64_t next_start = page_start;
        if (draw_page(&reader, page_start, &next_start) != 0) {
            println("pages: unable to read the file");
            status = 1;
            break;
        }

        uint16_t key = (uint16_t)get_key();
        if (key == 'q' || key == 'Q' || key == 0x1B) {
            break;
        }
        if (key == PAGES_KEY_UP || key == 'b' || key == 'B') {
            if (page_history_count > 0) {
                page_start = page_history[--page_history_count];
            }
            continue;
        }
        if (key == 'g') {
            if (page_start != 0) {
                remember_page(page_start);
                page_start = 0;
            }
            continue;
        }
        if (key == ' ' || key == 'f' || key == 'F' ||
            key == PAGES_KEY_DOWN) {
            if (next_start > page_start &&
                next_start < reader.file_size) {
                remember_page(page_start);
                page_start = next_start;
            }
        }
    }
    clear();
    return status;
}

void _start(int argc, char **argv) {
    if (argc == 1 && argv != NULL && argv[0] != NULL) {
        if (text_equal(argv[0], "--help")) {
            println("Usage: pages <file>");
            println("Page with Up/Down or Space/B; press G to return to the start.");
            println("Press Q to quit.");
            sys_exit(0);
        }
        if (text_equal(argv[0], "--version")) {
            println("WindogeOS pages 1.0");
            sys_exit(0);
        }
    }

    if (argc != 1 || argv == NULL || argv[0] == NULL ||
        argv[0][0] == '\0') {
        println("Usage: pages <file>");
        sys_exit(2);
        return;
    }

    sys_exit((uint64_t)view_file(argv[0]));
}
