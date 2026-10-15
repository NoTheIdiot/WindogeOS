#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/stddef.h"
#include "../library/bool.h"

#define TERMINAL_COLS 160
#define TERMINAL_ROWS 50

#define KEY_UP 0x101
#define KEY_DOWN 0x102
#define KEY_LEFT 0x103
#define KEY_RIGHT 0x104
#define KEY_BACKSPACE 0x008
#define KEY_ENTER 0x00A
#define KEY_CTRL_O 0x110
#define KEY_CTRL_N 0x111
#define KEY_CTRL_G 0x112
#define KEY_CTRL_F 0x113

#define MAX_LINES       1000
#define MAX_LINE_LEN    256
#define MAX_FILE_SIZE   8192

#define TOP_MARGIN      1  
#define BOTTOM_MARGIN   1  
#define LEFT_MARGIN     4  
#define VIEWPORT_ROWS   (TERMINAL_ROWS - TOP_MARGIN - BOTTOM_MARGIN)
#define VIEWPORT_COLS   (TERMINAL_COLS - LEFT_MARGIN)

static char lines[MAX_LINES][MAX_LINE_LEN]
    __attribute__((section(".data.editor"))) = {{1}};
static uint32_t line_lens[MAX_LINES]
    __attribute__((section(".data.editor"))) = {1};
static uint32_t total_lines __attribute__((section(".data.editor"))) = 1;

static uint32_t cursor_col __attribute__((section(".data.editor"))) = 0;
static uint32_t cursor_row __attribute__((section(".data.editor"))) = 0;

static uint32_t scroll_row __attribute__((section(".data.editor"))) = 0;
static uint32_t scroll_col __attribute__((section(".data.editor"))) = 0;

static char current_filename[128] __attribute__((section(".data.editor"))) =
    "untitled.txt";
static bool is_modified __attribute__((section(".data.editor"))) = false;
static bool should_quit __attribute__((section(".data.editor"))) = false;

static int text_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static int copy_text(char *destination, size_t capacity,
                     const char *source) {
    size_t length = str_strlen(source);
    if (length == 0 || length >= capacity) {
        return -1;
    }
    memcpy(destination, source, length + 1);
    return 0;
}

static void reset_editor_buffer(void) {
    for (size_t i = 0; i < MAX_LINES; i++) {
        lines[i][0] = '\0';
        line_lens[i] = 0;
    }
    total_lines = 1;
    cursor_col = 0;
    cursor_row = 0;
    scroll_row = 0;
    scroll_col = 0;
    is_modified = false;
}

static bool load_file_from_disk(const char *filename) {
    if (!filename || filename[0] == '\0') {
        return false;
    }

    size_t name_len = str_strlen(filename);
    if (name_len >= sizeof(current_filename)) {
        return false;
    }

    char requested_filename[sizeof(current_filename)];
    memcpy(requested_filename, filename, name_len + 1);

    static char raw_buf[MAX_FILE_SIZE]
        __attribute__((section(".data.editor"))) = {1};
    int64_t bytes_read = 0;
    if (file_exists(requested_filename) == 1) {
        bytes_read = (int64_t)read_file(requested_filename, raw_buf,
                                        sizeof(raw_buf) - 1);
        if (bytes_read < 0) {
            return false;
        }
        raw_buf[bytes_read] = '\0';
    }

    reset_editor_buffer();
    memcpy(current_filename, requested_filename, name_len + 1);
    if (bytes_read == 0) {
        return true;
    }

    uint32_t r = 0, c = 0;
    for (int64_t i = 0; i < bytes_read && r < MAX_LINES; i++) {
        char ch = raw_buf[i];
        if (ch == '\r') continue;

        if (ch == '\n') {
            lines[r][c] = '\0';
            line_lens[r] = c;
            r++;
            c = 0;
        } else if (c < MAX_LINE_LEN - 1) {
            lines[r][c++] = ch;
        }
    }

    if (r >= MAX_LINES) {
        total_lines = MAX_LINES;
        is_modified = false;
        return true;
    }

    lines[r][c] = '\0';
    line_lens[r] = c;
    total_lines = r + 1;
    is_modified = false;
    return true;
}

static bool save_file_to_disk(void) {
    static char save_buf[MAX_FILE_SIZE]
        __attribute__((section(".data.editor"))) = {1};
    uint32_t ptr = 0;

    for (uint32_t i = 0; i < total_lines; i++) {
        for (uint32_t j = 0; j < line_lens[i]; j++) {
            if (ptr < sizeof(save_buf) - 2) {
                save_buf[ptr++] = lines[i][j];
            }
        }
        if (i < total_lines - 1 && ptr < sizeof(save_buf) - 2) {
            save_buf[ptr++] = '\n';
        }
    }
    save_buf[ptr] = '\0';

    if (file_exists(current_filename) != 1 &&
        (int64_t)create_file(current_filename) < 0) {
        return false;
    }

    if ((int64_t)write_file(current_filename, save_buf) < 0) {
        return false;
    }

    is_modified = false;
    return true;
}

static void render_status_bar(void) {
    uint32_t status_y = TERMINAL_ROWS - 1;
    char blank_line[TERMINAL_COLS + 1];
    memset(blank_line, ' ', TERMINAL_COLS);
    blank_line[TERMINAL_COLS] = '\0';
    (void)background_color(COLOR_DARK_GRAY);
    (void)print_at(blank_line, 0, status_y, COLOR_WHITE);

    (void)print_at(current_filename, 1, status_y, COLOR_WHITE);
    if (is_modified) {
        (void)print_at("[*]", (uint32_t)(str_strlen(current_filename)) + 2,
                       status_y, COLOR_WHITE);
    }
    
    const char *controls = "^S: Save | ^X: Quit | ^O: Open | ^N: New | ^G: Goto | ^F: Find";
    (void)print_at(controls,
                   TERMINAL_COLS - (uint32_t)(str_strlen(controls)) - 1,
                   status_y, COLOR_WHITE);
    (void)background_color(COLOR_BLACK);
}

static bool status_bar_prompt(const char *prompt_label, char *buf, size_t buf_size) {
    size_t len = 0;
    buf[0] = '\0';

    while (true) {
        uint32_t status_y = TERMINAL_ROWS - 1;
        char blank_line[TERMINAL_COLS + 1];
        memset(blank_line, ' ', TERMINAL_COLS);
        blank_line[TERMINAL_COLS] = '\0';

        (void)background_color(COLOR_DARK_GRAY);
        (void)print_at(blank_line, 0, status_y, COLOR_WHITE);

        char display_buf[TERMINAL_COLS + 1];
        int pos = 0;
        for (int i = 0; prompt_label[i] != '\0' && pos < TERMINAL_COLS - 1; i++) {
            display_buf[pos++] = prompt_label[i];
        }
        for (size_t i = 0; i < len && pos < TERMINAL_COLS - 1; i++) {
            display_buf[pos++] = buf[i];
        }
        if (pos < TERMINAL_COLS - 1) {
            display_buf[pos++] = '_';
        }
        display_buf[pos] = '\0';

        (void)print_at(display_buf, 1, status_y, COLOR_GRAY);
        (void)background_color(COLOR_BLACK);

        uint16_t key = (uint16_t)get_key();
        if (key == KEY_ENTER || key == 0x0A) {
            buf[len] = '\0';
            return true;
        } else if (key == 0x1B) { // Escape
            return false;
        } else if (key == KEY_BACKSPACE || key == 0x08) {
            if (len > 0) {
                len--;
                buf[len] = '\0';
            }
        } else if (key >= 32 && key <= 126) {
            if (len + 1 < buf_size) {
                buf[len++] = (char)key;
                buf[len] = '\0';
            }
        }
    }
}

static void render_editor(void) {
    if (cursor_row < scroll_row) {
        scroll_row = cursor_row;
    }
    if (cursor_row >= scroll_row + VIEWPORT_ROWS) {
        scroll_row = cursor_row - VIEWPORT_ROWS + 1;
    }
    if (cursor_col < scroll_col) {
        scroll_col = cursor_col;
    }
    if (cursor_col >= scroll_col + VIEWPORT_COLS) {
        scroll_col = cursor_col - VIEWPORT_COLS + 1;
    }

    char line_buf[VIEWPORT_COLS + 1];

    for (uint32_t screen_y = 0; screen_y < VIEWPORT_ROWS; screen_y++) {
        uint32_t file_y = scroll_row + screen_y;
        uint32_t target_y = screen_y + TOP_MARGIN;

        if (file_y < total_lines) {
            char num_str[LEFT_MARGIN + 1];
            for (int i = 0; i < LEFT_MARGIN; i++) num_str[i] = ' ';
            num_str[LEFT_MARGIN] = '\0';

            uint32_t val = file_y + 1;
            char temp[10];
            int t = 0;
            while (val > 0 && t < 9) {
                temp[t++] = '0' + (val % 10);
                val /= 10;
            }
            for (int i = 0; i < t && i < LEFT_MARGIN; i++) {
                num_str[i] = temp[t - 1 - i];
            }
            (void)print_at(num_str, 0, target_y, COLOR_CYAN);
        } else {
            char spaces[LEFT_MARGIN + 1];
            for (int i = 0; i < LEFT_MARGIN; i++) spaces[i] = ' ';
            spaces[LEFT_MARGIN] = '\0';
            (void)print_at(spaces, 0, target_y, COLOR_BLACK);
        }

        uint32_t char_idx = 0;
        if (file_y < total_lines) {
            uint32_t len = line_lens[file_y];
            if (scroll_col < len) {
                uint32_t render_len = len - scroll_col;
                if (render_len > VIEWPORT_COLS) {
                    render_len = VIEWPORT_COLS;
                }
                for (uint32_t x = 0; x < render_len; x++) {
                    line_buf[char_idx++] = lines[file_y][scroll_col + x];
                }
            }
        }

        while (char_idx < VIEWPORT_COLS) {
            line_buf[char_idx++] = ' ';
        }
        line_buf[VIEWPORT_COLS] = '\0';

        (void)print_at(line_buf, LEFT_MARGIN, target_y, COLOR_WHITE);
    }

    render_status_bar();

    uint32_t vis_cursor_x = (cursor_col - scroll_col) + LEFT_MARGIN;
    uint32_t vis_cursor_y = (cursor_row - scroll_row) + TOP_MARGIN;

    if (vis_cursor_x >= LEFT_MARGIN && vis_cursor_x < TERMINAL_COLS && 
        vis_cursor_y >= TOP_MARGIN && vis_cursor_y < TERMINAL_ROWS - BOTTOM_MARGIN) {
        char c = ' ';
        if (cursor_row < total_lines && cursor_col < line_lens[cursor_row]) {
            c = lines[cursor_row][cursor_col];
        }
        char cur_str[2] = { c, '\0' };
        (void)background_color(COLOR_WHITE);
        (void)print_at(cur_str, vis_cursor_x, vis_cursor_y, COLOR_BLACK);
        (void)background_color(COLOR_BLACK);
    }
}

static void insert_char(char c) {
    if (line_lens[cursor_row] >= MAX_LINE_LEN - 1) return;

    for (int i = (int)line_lens[cursor_row]; i >= (int)cursor_col; i--) {
        lines[cursor_row][i + 1] = lines[cursor_row][i];
    }

    lines[cursor_row][cursor_col] = c;
    line_lens[cursor_row]++;
    cursor_col++;
    is_modified = true;
}

static void insert_newline(void) {
    if (total_lines >= MAX_LINES) return;

    for (int i = (int)total_lines; i > (int)cursor_row + 1; i--) {
        memcpy(lines[i], lines[i - 1], MAX_LINE_LEN);
        line_lens[i] = line_lens[i - 1];
    }

    uint32_t tail_len = line_lens[cursor_row] - cursor_col;
    memcpy(lines[cursor_row + 1], &lines[cursor_row][cursor_col], tail_len);
    lines[cursor_row + 1][tail_len] = '\0';
    line_lens[cursor_row + 1] = tail_len;

    lines[cursor_row][cursor_col] = '\0';
    line_lens[cursor_row] = cursor_col;

    total_lines++;
    cursor_row++;
    cursor_col = 0;
    is_modified = true;
}

static void delete_char(void) {
    if (cursor_col > 0) {
        for (uint32_t i = cursor_col - 1; i < line_lens[cursor_row]; i++) {
            lines[cursor_row][i] = lines[cursor_row][i + 1];
        }
        line_lens[cursor_row]--;
        cursor_col--;
        is_modified = true;
    } else if (cursor_row > 0) {
        uint32_t prev_len = line_lens[cursor_row - 1];
        uint32_t curr_len = line_lens[cursor_row];

        if (prev_len + curr_len < MAX_LINE_LEN) {
            memcpy(&lines[cursor_row - 1][prev_len], lines[cursor_row], curr_len + 1);
            line_lens[cursor_row - 1] += curr_len;

            for (uint32_t i = cursor_row; i < total_lines - 1; i++) {
                memcpy(lines[i], lines[i + 1], MAX_LINE_LEN);
                line_lens[i] = line_lens[i + 1];
            }

            total_lines--;
            cursor_row--;
            cursor_col = prev_len;
            is_modified = true;
        }
    }
}

static void editor_goto_line(void) {
    char buf[16];
    memset(buf, 0, sizeof(buf));
    if (!status_bar_prompt("Goto line: ", buf, sizeof(buf)) || buf[0] == '\0') {
        return;
    }
    uint32_t target = 0;
    for (int i = 0; buf[i] >= '0' && buf[i] <= '9'; i++) {
        target = target * (uint32_t)10 + (uint32_t)(buf[i] - '0');
    }
    if (target > 0 && target <= total_lines) {
        cursor_row = target - 1;
        if (cursor_col > line_lens[cursor_row]) {
            cursor_col = line_lens[cursor_row];
        }
    }
}

static void editor_find(void) {
    char query[64];
    memset(query, 0, sizeof(query));
    if (!status_bar_prompt("Find: ", query, sizeof(query)) || query[0] == '\0') {
        return;
    }

    uint32_t q_len = 0;
    while (query[q_len] != '\0') q_len++;
    if (q_len == 0) return;

    uint32_t start_r = cursor_row;
    uint32_t start_c = cursor_col + 1;

    for (uint32_t r = 0; r < total_lines; r++) {
        uint32_t check_r = (start_r + r) % total_lines;
        uint32_t line_len = line_lens[check_r];
        if (line_len == 0) continue;

        for (uint32_t c = 0; c < line_len; c++) {
            if (check_r == start_r && c < start_c && r == 0) continue;

            bool match = true;
            if (c + q_len > line_len) continue;

            for (uint32_t q = 0; q < q_len; q++) {
                if (lines[check_r][c + q] != query[q]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                cursor_row = check_r;
                cursor_col = c;
                return;
            }
        }
    }
}

static bool editor_confirm_changes(void) {
    if (!is_modified) {
        return true;
    }

    char answer[8];
    memset(answer, 0, sizeof(answer));
    if (!status_bar_prompt("Save changes? (y/n): ", answer, sizeof(answer)) || answer[0] == '\0') {
        return false;
    }
    if (answer[0] == 'y' || answer[0] == 'Y') {
        return save_file_to_disk();
    }
    return answer[0] == 'n' || answer[0] == 'N';
}

static void editor_open_file(void) {
    char filename[sizeof(current_filename)];
    memset(filename, 0, sizeof(filename));
    if (!status_bar_prompt("Open file: ", filename, sizeof(filename)) || filename[0] == '\0') {
        return;
    }
    if (file_exists(filename) != 1) {
        return;
    }
    if (!editor_confirm_changes()) {
        return;
    }
    (void)load_file_from_disk(filename);
}

static void editor_new_file(void) {
    char filename[sizeof(current_filename)];
    memset(filename, 0, sizeof(filename));
    if (!status_bar_prompt("New file name: ", filename, sizeof(filename)) || filename[0] == '\0') {
        return;
    }
    if (file_exists(filename) == 1) {
        return;
    }
    if (!editor_confirm_changes()) {
        return;
    }
    (void)load_file_from_disk(filename);
}

void _start(int argc, char **argv) {
    if (argc == 1 && argv != NULL && argv[0] != NULL &&
        (text_equal(argv[0], "--help") ||
         text_equal(argv[0], "--version"))) {
        println(text_equal(argv[0], "--help") ? "Usage: edit <file>"
                                              : "Dogeedit v2.3");
        sys_exit(0);
    }
    if (argc != 1 || argv == NULL || argv[0] == NULL ||
        argv[0][0] == '\0' ||
        copy_text(current_filename, sizeof(current_filename), argv[0]) != 0) {
        println("Usage: edit <file>");
        sys_exit(2);
    }

    (void)background_color(COLOR_BACKGROUND);
    (void)text_color(COLOR_GRAY);

    should_quit = false;
    if (!load_file_from_disk(current_filename)) {
        println("Unable to read file.");
        sys_exit(1);
    }

    while (!should_quit) {
        render_editor();

        uint16_t key = (uint16_t)get_key();

        switch (key) {
            case KEY_UP:
                if (cursor_row > 0) {
                    cursor_row--;
                    if (cursor_col > line_lens[cursor_row]) {
                        cursor_col = line_lens[cursor_row];
                    }
                }
                break;

            case KEY_DOWN:
                if (cursor_row < total_lines - 1) {
                    cursor_row++;
                    if (cursor_col > line_lens[cursor_row]) {
                        cursor_col = line_lens[cursor_row];
                    }
                }
                break;

            case KEY_LEFT:
                if (cursor_col > 0) {
                    cursor_col--;
                } else if (cursor_row > 0) {
                    cursor_row--;
                    cursor_col = line_lens[cursor_row];
                }
                break;

            case KEY_RIGHT:
                if (cursor_col < line_lens[cursor_row]) {
                    cursor_col++;
                } else if (cursor_row < total_lines - 1) {
                    cursor_row++;
                    cursor_col = 0;
                }
                break;

            case KEY_BACKSPACE:
                delete_char();
                break;

            case KEY_ENTER:
                insert_newline();
                break;

            case 0x13:
                (void)save_file_to_disk();
                break;

            case 0x18:
                should_quit = true;
                break;

            case KEY_CTRL_O:
                editor_open_file();
                break;

            case KEY_CTRL_N:
                editor_new_file();
                break;

            case 0x06: // ASCII Ctrl+F
            case KEY_CTRL_F:
                editor_find();
                break;

            case 0x07: // ASCII Ctrl+G
            case KEY_CTRL_G:
                editor_goto_line();
                break;

            default:
                if (key >= 32 && key <= 126) {
                    insert_char((char)key);
                }
                break;
        }
    }

    sys_exit(0);
}