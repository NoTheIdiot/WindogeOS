#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/dogelib.h"
#include "../library/stdint.h"
#include "../library/stddef.h"

#define MAX_ROWS 64
#define MAX_COLS 8
#define CELL_LEN 32
#define PATH_CAPACITY 256
#define FILE_BUF_SIZE 32768
#define VIEW_ROWS 17

typedef struct {
    char text[CELL_LEN];
} cell_t;

static cell_t sheet[MAX_ROWS][MAX_COLS];
static char sheet_file[PATH_CAPACITY];
static char file_buf[FILE_BUF_SIZE];
static char file_content[FILE_BUF_SIZE];

static void trim_newline(char* str) {
    if (!str) return;
    size_t len = str_strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ' || str[len - 1] == '\t')) {
        str[len - 1] = '\0';
        len--;
    }
}

static int ends_with_slk(const char* filename) {
    if (!filename) return 0;
    size_t len = str_strlen(filename);
    if (len < 4) return 0;
    
    const char* ext = filename + (len - 4);
    return (ext[0] == '.' && 
           (ext[1] == 's' || ext[1] == 'S') && 
           (ext[2] == 'l' || ext[2] == 'L') && 
           (ext[3] == 'k' || ext[3] == 'K'));
}

static void serialize_cell_display(const cell_t* cell, char* dest,
                                   size_t max_len) {
    char padded[13];
    memset(padded, ' ', sizeof(padded) - 1);
    padded[sizeof(padded) - 1] = '\0';

    int len = (int)str_strlen(cell->text);
    if (len > (int)sizeof(padded) - 1) {
        len = (int)sizeof(padded) - 1;
    }
    for (int i = 0; i < len; i++) {
        padded[i] = cell->text[i];
    }
    str_strncpy(dest, padded, max_len);
    dest[max_len - 1] = '\0';
}

static void parse_slk_line(const char* line) {
    const char* p = line;
    if (*p != 'C') return;
    p++;
    if (*p == ';') p++;

    int r = 0, c = 0;
    char val_buf[CELL_LEN];
    memset(val_buf, 0, sizeof(val_buf));
    int has_val = 0;

    while (*p != '\0' && *p != '\n' && *p != '\r') {
        if (*p == 'Y') {
            p++;
            r = 0;
            while (*p >= '0' && *p <= '9') {
                r = r * 10 + (*p - '0');
                p++;
            }
        } else if (*p == 'X') {
            p++;
            c = 0;
            while (*p >= '0' && *p <= '9') {
                c = c * 10 + (*p - '0');
                p++;
            }
        } else if (*p == 'K') {
            p++;
            if (*p == '"') p++;
            int v_idx = 0;
            while (*p != '"' && *p != '\n' && *p != '\r' && *p != '\0' && v_idx < CELL_LEN - 1) {
                val_buf[v_idx++] = *p;
                p++;
            }
            val_buf[v_idx] = '\0';
            if (*p == '"') p++;
            has_val = 1;
        } else {
            p++;
        }
    }

    if (has_val && r > 0 && r <= MAX_ROWS && c > 0 && c <= MAX_COLS) {
        str_strncpy(sheet[r - 1][c - 1].text, val_buf, CELL_LEN - 1);
        sheet[r - 1][c - 1].text[CELL_LEN - 1] = '\0';
    }
}

static void load_slk_file(void) {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            memset(sheet[r][c].text, 0, CELL_LEN);
        }
    }

    if (!file_exists(sheet_file)) return;

    memset(file_buf, 0, sizeof(file_buf));
    int bytes = (int)read_file(sheet_file, file_buf, sizeof(file_buf) - 1);
    if (bytes <= 0) return;

    file_buf[bytes] = '\0';
    char* line_start = file_buf;
    for (int i = 0; i <= bytes; i++) {
        if (file_buf[i] == '\n' || file_buf[i] == '\r' || file_buf[i] == '\0') {
            char old_char = file_buf[i];
            file_buf[i] = '\0';
            if (line_start[0] != '\0') {
                parse_slk_line(line_start);
            }
            file_buf[i] = old_char;
            line_start = &file_buf[i + 1];
        }
    }
}

static int append_text(char *destination, size_t capacity, size_t *length,
                       const char *text) {
    size_t text_length = str_strlen(text);
    if (*length >= capacity || text_length >= capacity - *length) {
        return 0;
    }
    memcpy(destination + *length, text, text_length);
    *length += text_length;
    destination[*length] = '\0';
    return 1;
}

static int save_slk_file(void) {
    memset(file_content, 0, sizeof(file_content));
    size_t content_length = 0;
    if (!append_text(file_content, sizeof(file_content), &content_length,
                     "ID;P\n")) {
        return 0;
    }

    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            if (sheet[r][c].text[0] != '\0') {
                char row_text[21];
                char column_text[21];
                str_u64toa((uint64_t)(r + 1), row_text);
                str_u64toa((uint64_t)(c + 1), column_text);
                if (!append_text(file_content, sizeof(file_content),
                                 &content_length, "C;Y") ||
                    !append_text(file_content, sizeof(file_content),
                                 &content_length, row_text) ||
                    !append_text(file_content, sizeof(file_content),
                                 &content_length, ";X") ||
                    !append_text(file_content, sizeof(file_content),
                                 &content_length, column_text) ||
                    !append_text(file_content, sizeof(file_content),
                                 &content_length, ";K\"") ||
                    !append_text(file_content, sizeof(file_content),
                                 &content_length, sheet[r][c].text) ||
                    !append_text(file_content, sizeof(file_content),
                                 &content_length, "\"\n")) {
                    return 0;
                }
            }
        }
    }
    if (!append_text(file_content, sizeof(file_content), &content_length,
                     "E\n")) {
        return 0;
    }

    return (int64_t)write_file_raw(sheet_file, (const uint8_t *)file_content,
                                   (uint64_t)content_length) >= 0;
}

void _start(void) {
    memset(sheet_file, 0, sizeof(sheet_file));

    background_color(COLOR_BACKGROUND);
    text_color(COLOR_BRIGHT_ORANGE);
    clear();

    print("  === Dogesheet v1.0 ===\n");
    text_color(COLOR_DARK_GRAY);
    print("  ---------------------------------------------------------\n");
    text_color(COLOR_WHITE);
    print("  Enter spreadsheet file path (.slk required): ");
    text_color(COLOR_BRIGHT_CYAN);
    input("", sheet_file, sizeof(sheet_file) - 1);
    trim_newline(sheet_file);

    if (!ends_with_slk(sheet_file) || !file_exists(sheet_file)) {
        clear();
        text_color(COLOR_BRIGHT_ORANGE);
        print("  === Dogesheet v1.0 ===\n");
        text_color(COLOR_DARK_GRAY);
        print("  ---------------------------------------------------------\n\n");
        
        if (!ends_with_slk(sheet_file)) {
            text_color(COLOR_BRIGHT_RED);
            print("  [ERROR] Invalid file format or missing .slk extension!\n");
        } else {
            text_color(COLOR_WHITE);
            print("  File not found: ");
            text_color(COLOR_BRIGHT_CYAN);
            print(sheet_file);
            print("\n");
        }

        text_color(COLOR_BRIGHT_GREEN);
        print("\n  [N] Make a new .slk file  |  [Q] Exit\n");

        while (1) {
            uint64_t key = get_key();
            if (key == 'q' || key == 'Q') {
                clear();
                exit(0);
            } else if (key == 'n' || key == 'N') {
                clear();
                text_color(COLOR_BRIGHT_ORANGE);
                print("  === Dogesheet v1.0 - New Sheet Setup ===\n");
                text_color(COLOR_DARK_GRAY);
                print("  ---------------------------------------------------------\n");
                
                char new_path[PATH_CAPACITY];
                memset(new_path, 0, sizeof(new_path));
                
                text_color(COLOR_WHITE);
                print("  Enter new .slk filename: ");
                text_color(COLOR_BRIGHT_CYAN);
                input("", new_path, sizeof(new_path) - 1);
                trim_newline(new_path);

                if (!ends_with_slk(new_path)) {
                    clear();
                    text_color(COLOR_BRIGHT_RED);
                    print("  [ERROR] File must end in .slk. Exiting...\n");
                    exit(0);
                }
                str_strcpy(sheet_file, new_path);

                for (int r = 0; r < MAX_ROWS; r++) {
                    for (int c = 0; c < MAX_COLS; c++) {
                        memset(sheet[r][c].text, 0, CELL_LEN);
                    }
                }

                if ((int64_t)create_file(sheet_file) < 0 ||
                    !save_slk_file()) {
                    println("Unable to create or save spreadsheet.");
                    exit(1);
                }
                break;
            }
        }
    } else {
        load_slk_file();
    }

    int cur_r = 0;
    int cur_c = 0;
    int first_visible_row = 0;
    int needs_redraw = 1;

    while (1) {
        if (needs_redraw) {
            clear();
            
            text_color(COLOR_BRIGHT_ORANGE);
            print("  [Dogesheet v1.0 - File: ");
            text_color(COLOR_BRIGHT_CYAN);
            print(sheet_file);
            text_color(COLOR_BRIGHT_ORANGE);
            print("]\n");
            text_color(COLOR_DARK_GRAY);
            print("  ==========================================================================================================\n");
            text_color(COLOR_BRIGHT_YELLOW);
            print("       |     A      |     B      |     C      |     D      |     E      |     F      |     G      |     H\n");
            text_color(COLOR_DARK_GRAY);
            print("  ==========================================================================================================\n");

            for (int visible_row = 0;
                 visible_row < VIEW_ROWS &&
                 first_visible_row + visible_row < MAX_ROWS;
                 visible_row++) {
                int r = first_visible_row + visible_row;
                uint32_t y = 5 + (uint32_t)visible_row;

                char row_number[21];
                char row_label[24] = " ";
                str_u64toa((uint64_t)(r + 1), row_number);
                str_strcat(row_label, row_number);
                str_strcat(row_label, " ");

                print_at(row_label, 0, y, COLOR_BRIGHT_GREEN);
                print_at("|", 5, y, COLOR_DARK_GRAY);

                for (int c = 0; c < MAX_COLS; c++) {
                    char cell_disp[16];
                    serialize_cell_display(&sheet[r][c], cell_disp, sizeof(cell_disp));

                    uint32_t x_pos = 7 + (uint32_t)(c * 13);
                    if (r == cur_r && c == cur_c) {
                        print_at(">", x_pos - 1, y, COLOR_CURSOR);
                        print_at(cell_disp, x_pos, y, COLOR_BRIGHT_CYAN);
                    } else {
                        print_at(" ", x_pos - 1, y, COLOR_BACKGROUND);
                        print_at(cell_disp, x_pos, y, COLOR_WHITE);
                    }
                    print_at("|", x_pos + 12, y, COLOR_DARK_GRAY);
                }
            }

            print_at("  ==========================================================================================================", 0, 22, COLOR_DARK_GRAY);
            print_at("  [W/S/A/D/Arrows] Move | [E] Edit | [Q] Save and exit", 0, 23, COLOR_BRIGHT_GREEN);
            print_at("  Selected value: ", 0, 24, COLOR_BRIGHT_YELLOW);
            print_at(sheet[cur_r][cur_c].text, 18, 24, COLOR_BRIGHT_CYAN);
            
            needs_redraw = 0;
        }

        uint64_t key = get_key();
        if (key == 0) {
            continue;
        }

        if (key == 'w' || key == 'W' || key == KEY_UP) {
            if (cur_r > 0) {
                cur_r--;
                needs_redraw = 1;
            }
        } 
        else if (key == 's' || key == 'S' || key == KEY_DOWN) {
            if (cur_r < MAX_ROWS - 1) {
                cur_r++;
                needs_redraw = 1;
            }
        }
        else if (key == 'a' || key == 'A' || key == KEY_LEFT) {
            if (cur_c > 0) {
                cur_c--;
                needs_redraw = 1;
            }
        }
        else if (key == 'd' || key == 'D' || key == KEY_RIGHT) {
            if (cur_c < MAX_COLS - 1) {
                cur_c++;
                needs_redraw = 1;
            }
        }
        else if (key == 'e' || key == 'E') {
            print("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
            
            text_color(COLOR_WHITE);
            print("  Edit [");
            text_color(COLOR_BRIGHT_YELLOW);
            char column_label[2] = {(char)('A' + cur_c), '\0'};
            print(column_label);
            
            char r_str[8];
            int r_val = cur_r + 1;
            int r_idx = 0;
            do { r_str[r_idx++] = (char)('0' + (r_val % 10)); r_val /= 10; } while (r_val > 0 && r_idx < 7);
            r_str[r_idx] = '\0';
            for (int i = 0; i < r_idx / 2; i++) {
                char tmp = r_str[i];
                r_str[i] = r_str[r_idx - 1 - i];
                r_str[r_idx - 1 - i] = tmp;
            }
            print(r_str);
            text_color(COLOR_WHITE);
            print("]: ");
            
            text_color(COLOR_BRIGHT_CYAN);
            input("", sheet[cur_r][cur_c].text, CELL_LEN - 1);
            trim_newline(sheet[cur_r][cur_c].text);
            sheet[cur_r][cur_c].text[CELL_LEN - 1] = '\0';
            
            if (!save_slk_file()) {
                println("\nUnable to save spreadsheet.");
                get_key();
            }
            needs_redraw = 1;
        }
        else if (key == 'q' || key == 'Q') {
            if (!save_slk_file()) {
                println("Unable to save spreadsheet.");
                exit(1);
            }
            clear();
            exit(0);
        }

        if (cur_r < first_visible_row) {
            first_visible_row = cur_r;
        } else if (cur_r >= first_visible_row + VIEW_ROWS) {
            first_visible_row = cur_r - VIEW_ROWS + 1;
        }
    }
}