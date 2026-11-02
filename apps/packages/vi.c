#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/stddef.h"

#define VI_COLS 160
#define VI_ROWS 50
#define VI_TOP_ROW 1
#define VI_CONTENT_ROWS (VI_ROWS - VI_TOP_ROW - 2)
#define VI_MAX_LINES 2048
#define VI_LINE_CAPACITY 256
#define VI_FILE_CAPACITY 65536
#define VI_PATH_CAPACITY 128
#define VI_COMMAND_CAPACITY 128

enum {
    VI_NORMAL,
    VI_INSERT,
    VI_COMMAND,
    VI_SEARCH
};

static char vi_lines[VI_MAX_LINES][VI_LINE_CAPACITY];
static uint32_t vi_line_lengths[VI_MAX_LINES];
static char vi_undo_lines[VI_MAX_LINES][VI_LINE_CAPACITY];
static uint32_t vi_undo_lengths[VI_MAX_LINES];
static char vi_file_buffer[VI_FILE_CAPACITY];
static char vi_save_buffer[VI_FILE_CAPACITY];
static char vi_filename[VI_PATH_CAPACITY];
static char vi_command[VI_COMMAND_CAPACITY];
static char vi_yank[VI_LINE_CAPACITY];
static char vi_search[VI_COMMAND_CAPACITY];
static char vi_status[VI_COLS + 1];

static uint32_t vi_line_count;
static uint32_t vi_cursor_row;
static uint32_t vi_cursor_col;
static uint32_t vi_scroll_row;
static uint32_t vi_scroll_col;
static uint32_t vi_undo_line_count;
static uint32_t vi_undo_cursor_row;
static uint32_t vi_undo_cursor_col;
static uint32_t vi_mode;
static uint32_t vi_pending_command;
static uint32_t vi_command_length;
static uint32_t vi_yank_length;
static int vi_modified;
static int vi_undo_modified;
static int vi_undo_valid;
static int vi_yank_valid;

static int vi_text_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static size_t vi_text_length(const char *text) {
    return str_strlen(text);
}

static int vi_copy_text(char *destination, size_t capacity,
                        const char *source) {
    if (destination == NULL || source == NULL || capacity == 0) {
        return -1;
    }
    size_t length = vi_text_length(source);
    if (length >= capacity) {
        return -1;
    }
    memcpy(destination, source, length + 1);
    return 0;
}

static void vi_set_status(const char *message) {
    (void)vi_copy_text(vi_status, sizeof(vi_status), message);
}

static void vi_clear_status(void) {
    vi_status[0] = '\0';
}

static uint32_t vi_document_size(void) {
    uint32_t size = vi_line_count == 0 ? 0 : vi_line_count - 1;
    for (uint32_t row = 0; row < vi_line_count; row++) {
        size += vi_line_lengths[row];
    }
    return size;
}

static void vi_begin_undo(void) {
    vi_undo_line_count = vi_line_count;
    vi_undo_cursor_row = vi_cursor_row;
    vi_undo_cursor_col = vi_cursor_col;
    vi_undo_modified = vi_modified;
    for (uint32_t row = 0; row < vi_line_count; row++) {
        memcpy(vi_undo_lines[row], vi_lines[row], VI_LINE_CAPACITY);
        vi_undo_lengths[row] = vi_line_lengths[row];
    }
    vi_undo_valid = 1;
}

static void vi_undo(void) {
    if (!vi_undo_valid) {
        vi_set_status("Already at oldest change");
        return;
    }
    vi_line_count = vi_undo_line_count;
    vi_cursor_row = vi_undo_cursor_row;
    vi_cursor_col = vi_undo_cursor_col;
    vi_modified = vi_undo_modified;
    for (uint32_t row = 0; row < vi_line_count; row++) {
        memcpy(vi_lines[row], vi_undo_lines[row], VI_LINE_CAPACITY);
        vi_line_lengths[row] = vi_undo_lengths[row];
    }
    vi_undo_valid = 0;
    vi_set_status("Change undone");
}

static int vi_load_file(const char *filename) {
    int64_t exists = (int64_t)file_exists(filename);
    if (exists < 0) {
        vi_set_status("vi: unable to check file");
        return -1;
    }

    vi_line_count = 1;
    vi_line_lengths[0] = 0;
    vi_lines[0][0] = '\0';
    vi_cursor_row = 0;
    vi_cursor_col = 0;
    vi_scroll_row = 0;
    vi_scroll_col = 0;
    vi_modified = 0;

    if (exists == 1) {
        dogec_stat_t file_info;
        file_info.size = 0;
        file_info.is_dir = 0;
        file_info.exists = 0;
        if ((int64_t)stat(filename, &file_info) < 0) {
            vi_set_status("vi: unable to inspect file");
            return -1;
        }
        if (file_info.size > sizeof(vi_file_buffer)) {
            vi_set_status("vi: file is too large or cannot be read");
            return -1;
        }
        int64_t bytes = (int64_t)read_file(filename, vi_file_buffer,
                                           sizeof(vi_file_buffer));
        if (bytes < 0 || (uint64_t)bytes != file_info.size) {
            vi_set_status("vi: failed to read file");
            return -1;
        }

        vi_line_count = 1;
        uint32_t column = 0;
        for (int64_t i = 0; i < bytes; i++) {
            char character = vi_file_buffer[i];
            if (character == '\r') {
                continue;
            }
            if (character == '\n') {
                vi_lines[vi_line_count - 1][column] = '\0';
                vi_line_lengths[vi_line_count - 1] = column;
                if (vi_line_count >= VI_MAX_LINES) {
                    vi_set_status("vi: file contains too many lines");
                    return -1;
                }
                vi_line_count++;
                vi_lines[vi_line_count - 1][0] = '\0';
                column = 0;
            } else {
                if (column >= VI_LINE_CAPACITY - 1) {
                    vi_set_status("vi: line exceeds 255 characters");
                    return -1;
                }
                vi_lines[vi_line_count - 1][column++] = character;
            }
        }
        vi_lines[vi_line_count - 1][column] = '\0';
        vi_line_lengths[vi_line_count - 1] = column;
    }

    vi_cursor_row = 0;
    vi_cursor_col = 0;
    vi_scroll_row = 0;
    vi_scroll_col = 0;
    vi_undo_valid = 0;
    return 0;
}

static int vi_save_file(const char *new_filename) {
    if (new_filename != NULL && new_filename[0] != '\0' &&
        vi_copy_text(vi_filename, sizeof(vi_filename), new_filename) != 0) {
        vi_set_status("vi: filename is too long");
        return -1;
    }
    if (vi_filename[0] == '\0') {
        vi_set_status("vi: no file name (use :w filename)");
        return -1;
    }

    uint32_t offset = 0;
    for (uint32_t row = 0; row < vi_line_count; row++) {
        if (vi_line_lengths[row] > sizeof(vi_save_buffer) - offset) {
            vi_set_status("vi: file exceeds 64 KiB");
            return -1;
        }
        memcpy(vi_save_buffer + offset, vi_lines[row],
               vi_line_lengths[row]);
        offset += vi_line_lengths[row];
        if (row + 1 < vi_line_count) {
            if (offset >= sizeof(vi_save_buffer)) {
                vi_set_status("vi: file exceeds 64 KiB");
                return -1;
            }
            vi_save_buffer[offset++] = '\n';
        }
    }

    int64_t exists = (int64_t)file_exists(vi_filename);
    if (exists < 0 ||
        (exists == 0 && (int64_t)create_file(vi_filename) < 0)) {
        vi_set_status("vi: unable to create file");
        return -1;
    }
    if ((int64_t)write_file_raw(vi_filename, (const uint8_t *)vi_save_buffer,
                                offset) < 0) {
        vi_set_status("vi: write failed");
        return -1;
    }
    vi_modified = 0;
    vi_set_status("\" written");
    return 0;
}

static void vi_clamp_cursor(void) {
    if (vi_line_count == 0) {
        vi_line_count = 1;
        vi_lines[0][0] = '\0';
        vi_line_lengths[0] = 0;
    }
    if (vi_cursor_row >= vi_line_count) {
        vi_cursor_row = vi_line_count - 1;
    }
    uint32_t length = vi_line_lengths[vi_cursor_row];
    if (vi_mode == VI_NORMAL && length > 0 && vi_cursor_col >= length) {
        vi_cursor_col = length - 1;
    } else if (vi_cursor_col > length) {
        vi_cursor_col = length;
    }
}

static int vi_insert_line(uint32_t row) {
    if (vi_line_count >= VI_MAX_LINES ||
        vi_document_size() >= VI_FILE_CAPACITY) {
        vi_set_status("vi: document limit reached");
        return -1;
    }
    for (uint32_t i = vi_line_count; i > row; i--) {
        memcpy(vi_lines[i], vi_lines[i - 1], VI_LINE_CAPACITY);
        vi_line_lengths[i] = vi_line_lengths[i - 1];
    }
    vi_lines[row][0] = '\0';
    vi_line_lengths[row] = 0;
    vi_line_count++;
    vi_modified = 1;
    return 0;
}

static int vi_insert_character(char character) {
    uint32_t length = vi_line_lengths[vi_cursor_row];
    if (length >= VI_LINE_CAPACITY - 1 ||
        vi_document_size() >= VI_FILE_CAPACITY) {
        vi_set_status("vi: line or file limit reached");
        return -1;
    }
    for (uint32_t i = length; i > vi_cursor_col; i--) {
        vi_lines[vi_cursor_row][i] = vi_lines[vi_cursor_row][i - 1];
    }
    vi_lines[vi_cursor_row][vi_cursor_col] = character;
    vi_line_lengths[vi_cursor_row]++;
    vi_lines[vi_cursor_row][vi_line_lengths[vi_cursor_row]] = '\0';
    vi_cursor_col++;
    vi_modified = 1;
    return 0;
}

static void vi_delete_at_cursor(void) {
    uint32_t length = vi_line_lengths[vi_cursor_row];
    if (length == 0 || vi_cursor_col >= length) {
        return;
    }
    vi_begin_undo();
    for (uint32_t i = vi_cursor_col; i + 1 < length; i++) {
        vi_lines[vi_cursor_row][i] = vi_lines[vi_cursor_row][i + 1];
    }
    vi_line_lengths[vi_cursor_row]--;
    vi_lines[vi_cursor_row][vi_line_lengths[vi_cursor_row]] = '\0';
    vi_modified = 1;
}

static void vi_delete_line(void) {
    if (vi_line_count == 1 && vi_line_lengths[0] == 0) {
        return;
    }
    vi_begin_undo();
    for (uint32_t i = vi_cursor_row; i + 1 < vi_line_count; i++) {
        memcpy(vi_lines[i], vi_lines[i + 1], VI_LINE_CAPACITY);
        vi_line_lengths[i] = vi_line_lengths[i + 1];
    }
    vi_line_count--;
    if (vi_line_count == 0) {
        vi_line_count = 1;
        vi_line_lengths[0] = 0;
        vi_lines[0][0] = '\0';
    }
    vi_cursor_col = 0;
    vi_modified = 1;
    vi_clamp_cursor();
}

static void vi_join_lines(void) {
    if (vi_cursor_row + 1 >= vi_line_count) {
        return;
    }
    uint32_t left = vi_line_lengths[vi_cursor_row];
    uint32_t right = vi_line_lengths[vi_cursor_row + 1];
    uint32_t separator = left > 0 && right > 0 ? 1U : 0U;
    if (left + separator + right >= VI_LINE_CAPACITY) {
        vi_set_status("vi: joined line would be too long");
        return;
    }
    vi_begin_undo();
    if (separator != 0) {
        vi_lines[vi_cursor_row][left++] = ' ';
    }
    memcpy(vi_lines[vi_cursor_row] + left,
           vi_lines[vi_cursor_row + 1], right + 1);
    vi_line_lengths[vi_cursor_row] = left + right;
    for (uint32_t i = vi_cursor_row + 1; i + 1 < vi_line_count; i++) {
        memcpy(vi_lines[i], vi_lines[i + 1], VI_LINE_CAPACITY);
        vi_line_lengths[i] = vi_line_lengths[i + 1];
    }
    vi_line_count--;
    vi_modified = 1;
}

static void vi_backspace_insert(void) {
    if (vi_cursor_col > 0) {
        for (uint32_t i = vi_cursor_col - 1;
             i + 1 < vi_line_lengths[vi_cursor_row]; i++) {
            vi_lines[vi_cursor_row][i] = vi_lines[vi_cursor_row][i + 1];
        }
        vi_line_lengths[vi_cursor_row]--;
        vi_cursor_col--;
        vi_lines[vi_cursor_row][vi_line_lengths[vi_cursor_row]] = '\0';
        vi_modified = 1;
    } else if (vi_cursor_row > 0) {
        uint32_t previous_length = vi_line_lengths[vi_cursor_row - 1];
        uint32_t current_length = vi_line_lengths[vi_cursor_row];
        if (previous_length + current_length >= VI_LINE_CAPACITY) {
            vi_set_status("vi: joined line would be too long");
            return;
        }
        vi_begin_undo();
        memcpy(vi_lines[vi_cursor_row - 1] + previous_length,
               vi_lines[vi_cursor_row], current_length + 1);
        vi_line_lengths[vi_cursor_row - 1] += current_length;
        for (uint32_t i = vi_cursor_row; i + 1 < vi_line_count; i++) {
            memcpy(vi_lines[i], vi_lines[i + 1], VI_LINE_CAPACITY);
            vi_line_lengths[i] = vi_line_lengths[i + 1];
        }
        vi_line_count--;
        vi_cursor_row--;
        vi_cursor_col = previous_length;
        vi_modified = 1;
    }
}

static void vi_start_insert(uint32_t column, int save_undo) {
    if (save_undo) {
        vi_begin_undo();
    }
    vi_cursor_col = column;
    vi_mode = VI_INSERT;
    vi_clear_status();
}

static void vi_open_line(uint32_t row) {
    vi_begin_undo();
    if (vi_insert_line(row) == 0) {
        vi_cursor_row = row;
        vi_cursor_col = 0;
        vi_mode = VI_INSERT;
        vi_undo_valid = 1;
    }
}

static void vi_move_word_forward(void) {
    uint32_t row = vi_cursor_row;
    uint32_t column = vi_cursor_col;
    while (row < vi_line_count) {
        uint32_t length = vi_line_lengths[row];
        while (column < length && vi_lines[row][column] != ' ' &&
               vi_lines[row][column] != '\t') {
            column++;
        }
        while (column < length && (vi_lines[row][column] == ' ' ||
                                   vi_lines[row][column] == '\t')) {
            column++;
        }
        if (column < length) {
            vi_cursor_row = row;
            vi_cursor_col = column;
            return;
        }
        row++;
        column = 0;
    }
    vi_cursor_row = vi_line_count - 1;
    vi_cursor_col = vi_line_lengths[vi_cursor_row] == 0
                        ? 0
                        : vi_line_lengths[vi_cursor_row] - 1;
}

static void vi_move_word_backward(void) {
    if (vi_cursor_row == 0 && vi_cursor_col == 0) {
        return;
    }
    uint32_t row = vi_cursor_row;
    uint32_t column = vi_cursor_col;
    if (column > 0) {
        column--;
    } else {
        row--;
        column = vi_line_lengths[row];
        if (column > 0) {
            column--;
        }
    }
    while (row > 0 || column > 0) {
        if (vi_lines[row][column] != ' ' && vi_lines[row][column] != '\t') {
            break;
        }
        if (column > 0) {
            column--;
        } else {
            row--;
            column = vi_line_lengths[row] == 0
                         ? 0
                         : vi_line_lengths[row] - 1;
        }
    }
    while (column > 0 && vi_lines[row][column - 1] != ' ' &&
           vi_lines[row][column - 1] != '\t') {
        column--;
    }
    vi_cursor_row = row;
    vi_cursor_col = column;
}

static void vi_search_forward(void) {
    size_t needle_length = vi_text_length(vi_search);
    if (needle_length == 0) {
        vi_set_status("No previous search");
        return;
    }
    for (uint32_t row_offset = 0; row_offset < vi_line_count; row_offset++) {
        uint32_t row = (vi_cursor_row + row_offset) % vi_line_count;
        uint32_t start = row == vi_cursor_row ? vi_cursor_col + 1 : 0;
        if (row_offset > 0 && row < vi_cursor_row) {
            break;
        }
        for (uint32_t column = start;
             column + needle_length <= vi_line_lengths[row]; column++) {
            if (memcmp(vi_lines[row] + column, vi_search, needle_length) == 0) {
                vi_cursor_row = row;
                vi_cursor_col = column;
                vi_clear_status();
                return;
            }
        }
    }
    for (uint32_t row = 0; row <= vi_cursor_row; row++) {
        for (uint32_t column = 0;
             column + needle_length <= vi_line_lengths[row]; column++) {
            if ((row < vi_cursor_row || column < vi_cursor_col) &&
                memcmp(vi_lines[row] + column, vi_search, needle_length) == 0) {
                vi_cursor_row = row;
                vi_cursor_col = column;
                vi_clear_status();
                return;
            }
        }
    }
    vi_set_status("Pattern not found");
}

static void vi_render(void) {
    if (vi_cursor_row < vi_scroll_row) {
        vi_scroll_row = vi_cursor_row;
    } else if (vi_cursor_row >= vi_scroll_row + VI_CONTENT_ROWS) {
        vi_scroll_row = vi_cursor_row - VI_CONTENT_ROWS + 1;
    }
    if (vi_cursor_col < vi_scroll_col) {
        vi_scroll_col = vi_cursor_col;
    } else if (vi_cursor_col >= vi_scroll_col + VI_COLS) {
        vi_scroll_col = vi_cursor_col - VI_COLS + 1;
    }

    char row_buffer[VI_COLS + 1];
    for (uint32_t screen_row = 0; screen_row < VI_CONTENT_ROWS; screen_row++) {
        uint32_t row = vi_scroll_row + screen_row;
        if (row >= vi_line_count) {
            row_buffer[0] = '~';
            memset(row_buffer + 1, ' ', VI_COLS - 1);
        } else {
            uint32_t count = 0;
            uint32_t length = vi_line_lengths[row];
            for (uint32_t column = vi_scroll_col;
                 column < length && count < VI_COLS; column++) {
                row_buffer[count++] = vi_lines[row][column];
            }
            memset(row_buffer + count, ' ', VI_COLS - count);
        }
        row_buffer[VI_COLS] = '\0';
        (void)print_at(row_buffer, 0, screen_row + VI_TOP_ROW, COLOR_WHITE);
    }

    char status_line[VI_COLS + 1];
    memset(status_line, ' ', VI_COLS);
    status_line[VI_COLS] = '\0';
    (void)background_color(COLOR_DARK_GRAY);
    (void)print_at(status_line, 0, VI_ROWS - 2, COLOR_WHITE);
    (void)print_at(vi_filename[0] == '\0' ? "[No Name]" : vi_filename,
                   0, VI_ROWS - 2, COLOR_WHITE);
    uint32_t status_x = (uint32_t)vi_text_length(vi_filename);
    if (vi_filename[0] == '\0') {
        status_x = 9;
    }
    if (vi_modified && status_x + 2 < VI_COLS) {
        (void)print_at("[+]", status_x, VI_ROWS - 2, COLOR_BRIGHT_YELLOW);
    }
    const char *mode_name = vi_mode == VI_INSERT ? "-- INSERT --" : "";
    if (mode_name[0] != '\0') {
        (void)print_at(mode_name, VI_COLS - 14, VI_ROWS - 2,
                       COLOR_BRIGHT_CYAN);
    }
    (void)background_color(COLOR_BLACK);

    char message_line[VI_COLS + 1];
    memset(message_line, ' ', VI_COLS);
    message_line[VI_COLS] = '\0';
    if (vi_mode == VI_COMMAND || vi_mode == VI_SEARCH) {
        message_line[0] = vi_mode == VI_COMMAND ? ':' : '/';
        memcpy(message_line + 1, vi_command, vi_command_length);
        if (vi_command_length < VI_COLS - 1) {
            message_line[vi_command_length + 1] = '_';
        }
    } else if (vi_status[0] != '\0') {
        memcpy(message_line, vi_status,
               vi_text_length(vi_status) < VI_COLS
                   ? vi_text_length(vi_status)
                   : VI_COLS);
    } else {
        uint32_t row_number = vi_cursor_row + 1;
        char digits[12];
        uint32_t digit_count = 0;
        do {
            digits[digit_count++] = (char)('0' + row_number % 10);
            row_number /= 10;
        } while (row_number != 0 && digit_count < sizeof(digits));
        uint32_t pos = 0;
        while (digit_count > 0) {
            message_line[pos++] = digits[--digit_count];
        }
        message_line[pos++] = ',';
        message_line[pos++] = ' ';
        uint32_t column_number = vi_cursor_col + 1;
        digit_count = 0;
        do {
            digits[digit_count++] = (char)('0' + column_number % 10);
            column_number /= 10;
        } while (column_number != 0 && digit_count < sizeof(digits));
        while (digit_count > 0 && pos < VI_COLS) {
            message_line[pos++] = digits[--digit_count];
        }
    }
    (void)print_at(message_line, 0, VI_ROWS - 1, COLOR_GRAY);

    uint32_t screen_x = vi_cursor_col - vi_scroll_col;
    uint32_t screen_y = vi_cursor_row - vi_scroll_row + VI_TOP_ROW;
    if (screen_x < VI_COLS && screen_y < VI_CONTENT_ROWS) {
        char cursor_character = ' ';
        if (vi_cursor_col < vi_line_lengths[vi_cursor_row]) {
            cursor_character = vi_lines[vi_cursor_row][vi_cursor_col];
        }
        char cursor_text[2] = {cursor_character, '\0'};
        (void)background_color(COLOR_WHITE);
        (void)print_at(cursor_text, screen_x, screen_y, COLOR_BLACK);
        (void)background_color(COLOR_BLACK);
    }
}

static void vi_execute_command(void) {
    vi_command[vi_command_length] = '\0';
    if (vi_mode == VI_SEARCH) {
        if (vi_command_length > 0 &&
            vi_copy_text(vi_search, sizeof(vi_search), vi_command) == 0) {
            vi_search_forward();
        }
        vi_mode = VI_NORMAL;
        return;
    }

    char *argument = vi_command;
    while (*argument != '\0' && *argument != ' ' && *argument != '\t') {
        argument++;
    }
    char delimiter = *argument;
    *argument = '\0';
    if (delimiter != '\0') {
        argument++;
        while (*argument == ' ' || *argument == '\t') {
            argument++;
        }
    }

    if (vi_text_equal(vi_command, "w") ||
        vi_text_equal(vi_command, "write")) {
        (void)vi_save_file(argument);
    } else if (vi_text_equal(vi_command, "wq") ||
               vi_text_equal(vi_command, "x")) {
        if (vi_save_file(argument) == 0) {
            vi_mode = VI_NORMAL;
            vi_pending_command = 0x100;
            return;
        }
    } else if (vi_text_equal(vi_command, "q") ||
               vi_text_equal(vi_command, "quit")) {
        if (vi_modified) {
            vi_set_status("No write since last change (use :q! to override)");
        } else {
            vi_pending_command = 0x100;
        }
    } else if (vi_text_equal(vi_command, "q!")) {
        vi_modified = 0;
        vi_pending_command = 0x100;
    } else {
        vi_set_status("Not an editor command");
    }
    vi_mode = VI_NORMAL;
}

static void vi_handle_command_key(uint64_t key) {
    if (key == 0x1b) {
        vi_mode = VI_NORMAL;
        vi_command_length = 0;
        vi_clear_status();
    } else if (key == KEY_ENTER || key == '\n') {
        vi_execute_command();
    } else if (key == KEY_BACKSPACE || key == 0x7f || key == 0x08) {
        if (vi_command_length > 0) {
            vi_command[--vi_command_length] = '\0';
        }
    } else if (key >= 32 && key <= 126 &&
               vi_command_length + 1 < sizeof(vi_command)) {
        vi_command[vi_command_length++] = (char)key;
        vi_command[vi_command_length] = '\0';
    }
}

static void vi_handle_insert_key(uint64_t key) {
    if (key == 0x1b) {
        vi_mode = VI_NORMAL;
        if (vi_cursor_col > 0) {
            vi_cursor_col--;
        }
        vi_clamp_cursor();
        return;
    }
    if (key == KEY_BACKSPACE || key == 0x08 || key == 0x7f) {
        vi_backspace_insert();
        return;
    }
    if (key == KEY_ENTER || key == '\n' || key == '\r') {
        if (vi_insert_line(vi_cursor_row + 1) == 0) {
            uint32_t tail_length =
                vi_line_lengths[vi_cursor_row] - vi_cursor_col;
            memcpy(vi_lines[vi_cursor_row + 1],
                   vi_lines[vi_cursor_row] + vi_cursor_col,
                   tail_length);
            vi_lines[vi_cursor_row + 1][tail_length] = '\0';
            vi_line_lengths[vi_cursor_row + 1] = tail_length;
            vi_lines[vi_cursor_row][vi_cursor_col] = '\0';
            vi_line_lengths[vi_cursor_row] = vi_cursor_col;
            vi_cursor_row++;
            vi_cursor_col = 0;
        }
        return;
    }
    if (key == KEY_LEFT) {
        if (vi_cursor_col > 0) vi_cursor_col--;
        return;
    }
    if (key == KEY_RIGHT) {
        if (vi_cursor_col < vi_line_lengths[vi_cursor_row]) vi_cursor_col++;
        return;
    }
    if (key == KEY_UP) {
        if (vi_cursor_row > 0) vi_cursor_row--;
        vi_clamp_cursor();
        return;
    }
    if (key == KEY_DOWN) {
        if (vi_cursor_row + 1 < vi_line_count) vi_cursor_row++;
        vi_clamp_cursor();
        return;
    }
    if ((key >= 32 && key <= 126) || key == KEY_TAB) {
        (void)vi_insert_character(key == KEY_TAB ? '\t' : (char)key);
    }
}

static void vi_handle_normal_key(uint64_t key) {
    if (vi_pending_command == 0x100) {
        vi_pending_command = 0;
        return;
    }
    if (vi_pending_command == 'g') {
        vi_pending_command = 0;
        if (key == 'g') {
            vi_cursor_row = 0;
            vi_cursor_col = 0;
            return;
        }
    } else if (vi_pending_command == 'd' || vi_pending_command == 'y') {
        uint32_t command = vi_pending_command;
        vi_pending_command = 0;
        if (key == command) {
            if (command == 'd') {
                vi_delete_line();
            } else {
                vi_yank_length = vi_line_lengths[vi_cursor_row];
                memcpy(vi_yank, vi_lines[vi_cursor_row],
                       vi_yank_length + 1);
                vi_yank_valid = 1;
                vi_set_status("1 line yanked");
            }
        }
        return;
    }
    if (key == 'd' || key == 'y') {
        vi_pending_command = (uint32_t)key;
        return;
    }
    switch (key) {
        case KEY_UP:
        case 'k':
            if (vi_cursor_row > 0) vi_cursor_row--;
            vi_clamp_cursor();
            break;
        case KEY_DOWN:
        case 'j':
            if (vi_cursor_row + 1 < vi_line_count) vi_cursor_row++;
            vi_clamp_cursor();
            break;
        case KEY_LEFT:
        case 'h':
            if (vi_cursor_col > 0) vi_cursor_col--;
            break;
        case KEY_RIGHT:
        case 'l':
            if (vi_cursor_col + 1 < vi_line_lengths[vi_cursor_row]) {
                vi_cursor_col++;
            }
            break;
        case '0':
            vi_cursor_col = 0;
            break;
        case '$':
            vi_cursor_col = vi_line_lengths[vi_cursor_row] == 0
                                ? 0
                                : vi_line_lengths[vi_cursor_row] - 1;
            break;
        case 'w':
            vi_move_word_forward();
            break;
        case 'b':
            vi_move_word_backward();
            break;
        case 'G':
            vi_cursor_row = vi_line_count - 1;
            vi_clamp_cursor();
            break;
        case 'g':
            vi_pending_command = 'g';
            break;
        case 'i':
            vi_start_insert(vi_cursor_col, 1);
            break;
        case 'a':
            vi_start_insert(vi_cursor_col < vi_line_lengths[vi_cursor_row]
                                ? vi_cursor_col + 1
                                : vi_cursor_col, 1);
            break;
        case 'I':
            vi_start_insert(0, 1);
            break;
        case 'A':
            vi_start_insert(vi_line_lengths[vi_cursor_row], 1);
            break;
        case 'o':
            vi_open_line(vi_cursor_row + 1);
            break;
        case 'O':
            vi_open_line(vi_cursor_row);
            break;
        case 'x':
            vi_delete_at_cursor();
            vi_clamp_cursor();
            break;
        case 'X':
            if (vi_cursor_col > 0) {
                vi_cursor_col--;
                vi_delete_at_cursor();
            }
            break;
        case 'J':
            vi_join_lines();
            break;
        case 'p':
        case 'P':
            if (vi_yank_valid) {
                uint32_t target = key == 'p' ? vi_cursor_row + 1
                                             : vi_cursor_row;
                if (vi_line_count >= VI_MAX_LINES ||
                    vi_document_size() + 1 + vi_yank_length >
                        VI_FILE_CAPACITY) {
                    vi_set_status("vi: document limit reached");
                    break;
                }
                vi_begin_undo();
                if (vi_insert_line(target) == 0) {
                    memcpy(vi_lines[target], vi_yank, vi_yank_length + 1);
                    vi_line_lengths[target] = vi_yank_length;
                    vi_cursor_row = target;
                    vi_cursor_col = 0;
                }
            }
            break;
        case 'u':
            vi_undo();
            break;
        case ':':
        case '/':
            vi_mode = key == ':' ? VI_COMMAND : VI_SEARCH;
            vi_command_length = 0;
            vi_command[0] = '\0';
            vi_clear_status();
            break;
        case 'n':
            vi_search_forward();
            break;
        case 0x1b:
            vi_pending_command = 0;
            vi_clear_status();
            break;
        default:
            vi_pending_command = 0;
            break;
    }
}

void _start(int argc, char **argv) {
    if (argc < 0 || argc > 1) {
        println("Usage: vi [file]");
        exit(2);
        return;
    }

    vi_filename[0] = '\0';
    if (argc == 1) {
        if (argv == NULL || argv[0] == NULL || argv[0][0] == '\0') {
            println("Usage: vi [file]");
            exit(2);
            return;
        }
        if (vi_copy_text(vi_filename, sizeof(vi_filename), argv[0]) != 0) {
            println("vi: filename is too long");
            exit(2);
            return;
        }
    }

    (void)set_text_mode();
    (void)background_color(COLOR_BLACK);
    (void)text_color(COLOR_WHITE);
    if (vi_filename[0] != '\0' && vi_load_file(vi_filename) != 0) {
        println(vi_status);
        exit(1);
        return;
    }
    if (vi_filename[0] == '\0') {
        vi_line_count = 1;
        vi_line_lengths[0] = 0;
        vi_lines[0][0] = '\0';
        vi_cursor_row = 0;
        vi_cursor_col = 0;
    }
    vi_mode = VI_NORMAL;
    vi_status[0] = '\0';
    vi_pending_command = 0;

    for (;;) {
        vi_render();
        uint64_t key = get_key();
        if (key == 0) {
            continue;
        }
        if (vi_mode == VI_INSERT) {
            vi_handle_insert_key(key);
        } else if (vi_mode == VI_COMMAND || vi_mode == VI_SEARCH) {
            vi_handle_command_key(key);
        } else {
            vi_handle_normal_key(key);
        }
        if (vi_pending_command == 0x100) {
            clear();
            exit(0);
            return;
        }
    }
}
