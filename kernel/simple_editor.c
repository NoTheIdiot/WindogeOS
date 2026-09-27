#include <string.h>
#include <bool.h>
#include <system.h>
#include <dogeio.h>

#define MAX_LINES       1000
#define MAX_LINE_LEN    256

#define TOP_MARGIN      1  
#define BOTTOM_MARGIN   1  
#define VIEWPORT_ROWS   (TERMINAL_ROWS - TOP_MARGIN - BOTTOM_MARGIN)
#define VIEWPORT_COLS   TERMINAL_COLS

static char lines[MAX_LINES][MAX_LINE_LEN];
static uint32_t line_lens[MAX_LINES];
static uint32_t total_lines = 1;

static uint32_t cursor_col = 0; 
static uint32_t cursor_row = 0; 

static uint32_t scroll_row = 0; 
static uint32_t scroll_col = 0; 

static char current_filename[128] = "untitled.txt";
static bool is_modified = false;
static bool should_quit = false;

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

static void load_file_from_disk(const char *filename) {
    reset_editor_buffer();
    
    if (filename && filename[0] != '\0') {
        size_t name_len = str_strlen(filename);
        if (name_len >= sizeof(current_filename)) {
            name_len = sizeof(current_filename) - 1;
        }
        memcpy(current_filename, filename, name_len);
        current_filename[name_len] = '\0';
    }

    if (!fs_exists(current_filename)) {
        return; 
    }
    
    static char raw_buf[8192];
    int bytes_read = fs_read(current_filename, raw_buf, sizeof(raw_buf) - 1);
    if (bytes_read <= 0) {
        return;
    }
    raw_buf[bytes_read] = '\0';

    uint32_t r = 0, c = 0;
    for (int i = 0; i < bytes_read && r < MAX_LINES; i++) {
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

    lines[r][c] = '\0';
    line_lens[r] = c;
    total_lines = r + 1;
    is_modified = false;
}

static bool save_file_to_disk(void) {
    static char save_buf[8192];
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

    if (fs_exists(current_filename)) {
        fs_delete(current_filename);
    }

    if (fs_create(current_filename) < 0) {
        return false;
    }

    if (fs_write(current_filename, save_buf) < 0) {
        return false;
    }

    is_modified = false;
    return true;
}

static void render_status_bar(void) {
    uint32_t orig_bg = dogeio_background_color;
    uint32_t orig_fg = dogeio_text_color;

    dogeio_text_background_change(COLOR_DARK_GRAY);
    dogeio_text_color_change(COLOR_WHITE);

    uint32_t status_y = TERMINAL_ROWS - 1;
    char blank_line[TERMINAL_COLS + 1];
    memset(blank_line, ' ', TERMINAL_COLS);
    blank_line[TERMINAL_COLS] = '\0';
    dogeio_text_print_at(blank_line, 0, status_y, COLOR_WHITE);

    dogeio_text_print_at(current_filename, 1, status_y, COLOR_WHITE);
    if (is_modified) {
        dogeio_text_print_at("[*]", (uint32_t)(str_strlen(current_filename)) + 2, status_y, COLOR_DARK_GRAY);
    }
    
    const char *controls = "^S: Save | ^X: Quit";
    dogeio_text_print_at(controls, TERMINAL_COLS - (uint32_t)(str_strlen(controls)) - 1, status_y, COLOR_WHITE);

    dogeio_text_background_change(orig_bg);
    dogeio_text_color_change(orig_fg);
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

    char line_buf[TERMINAL_COLS + 1];

    for (uint32_t screen_y = 0; screen_y < VIEWPORT_ROWS; screen_y++) {
        uint32_t file_y = scroll_row + screen_y;
        uint32_t target_y = screen_y + TOP_MARGIN;
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

        dogeio_text_print_at(line_buf, 0, target_y, dogeio_text_color);
    }

    render_status_bar();

    uint32_t vis_cursor_x = cursor_col - scroll_col;
    uint32_t vis_cursor_y = (cursor_row - scroll_row) + TOP_MARGIN;

    if (vis_cursor_x < VIEWPORT_COLS && vis_cursor_y >= TOP_MARGIN && vis_cursor_y < TERMINAL_ROWS - BOTTOM_MARGIN) {
        char c = ' ';
        if (cursor_row < total_lines && cursor_col < line_lens[cursor_row]) {
            c = lines[cursor_row][cursor_col];
        }
        char cur_str[2] = { c, '\0' };
        dogeio_text_background_change(COLOR_WHITE);
        dogeio_text_print_at(cur_str, vis_cursor_x, vis_cursor_y, COLOR_BLACK);
        dogeio_text_background_change(COLOR_BLACK);
        dogeio_text_color_change(COLOR_WHITE);
    }

    dogeio_cursor_visible = false;
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

void system_editor(char* filename) {
    if (!filename || filename[0] == '\0') {
        dogeio_text_println("Error: Invalid filename.");
        return;
    }

    dogeio_text_background_change(COLOR_BLACK);
    dogeio_text_color_change(COLOR_WHITE);

    should_quit = false;
    load_file_from_disk(filename);

    while (!should_quit) {
        render_editor();

        uint16_t key = dogeio_get_key();

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
                save_file_to_disk();
                break;

            case 0x18:
                should_quit = true;
                break;

            default:
                if (key >= 32 && key <= 126) {
                    insert_char((char)key);
                }
                break;
        }
    }

    dogeio_text_clear();
}