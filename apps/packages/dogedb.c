#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/dogelib.h"
#include "../library/stdint.h"
#include "../library/stddef.h"

#define MAX_RECORDS 64
#define MAX_COLS 5
#define FIELD_LEN 16
#define PATH_CAPACITY 256

typedef struct {
    char fields[MAX_COLS][FIELD_LEN];
} db_row_t;

static char headers[MAX_COLS][FIELD_LEN];

static int ends_with_csv(const char* filename) {
    if (!filename) return 0;
    size_t len = str_strlen(filename);
    if (len < 4) return 0;
    
    const char* ext = filename + (len - 4);
    return (ext[0] == '.' && 
           (ext[1] == 'c' || ext[1] == 'C') && 
           (ext[2] == 's' || ext[2] == 'S') && 
           (ext[3] == 'v' || ext[3] == 'V'));
}

static uint64_t parse_csv_line(const char* line, db_row_t* row) {
    const char* p = line;
    int field = 0;
    int idx = 0;

    memset(row, 0, sizeof(db_row_t));

    while (*p != '\0' && *p != '\n' && *p != '\r') {
        if (*p == ',') {
            field++;
            idx = 0;
            if (field >= MAX_COLS) break;
        } else {
            if (field < MAX_COLS && idx < FIELD_LEN - 1) {
                row->fields[field][idx++] = *p;
            }
        }
        p++;
    }
    return field >= (MAX_COLS - 1) ? 0 : (uint64_t)-1;
}

static void serialize_row(const db_row_t* row, char* dest, size_t max_len) {
    char temp[128];
    memset(temp, 0, sizeof(temp));
    int cursor = 0;

    for (int c = 0; c < MAX_COLS; c++) {
        char padded[13];
        memset(padded, ' ', 12);
        padded[12] = '\0';

        int len = (int)str_strlen(row->fields[c]);
        if (len > 12) len = 12;
        for (int i = 0; i < len; i++) {
            padded[i] = row->fields[c][i];
        }

        for (int i = 0; i < 12; i++) {
            temp[cursor++] = padded[i];
        }

        if (c < MAX_COLS - 1) {
            temp[cursor++] = ' ';
            temp[cursor++] = '|';
            temp[cursor++] = ' ';
        }
    }
    temp[cursor] = '\0';
    str_strncpy(dest, temp, max_len);
}

static void serialize_headers(char* dest, size_t max_len) {
    char temp[128];
    memset(temp, 0, sizeof(temp));
    int cursor = 0;

    for (int c = 0; c < MAX_COLS; c++) {
        char padded[13];
        memset(padded, ' ', 12);
        padded[12] = '\0';

        int len = (int)str_strlen(headers[c]);
        if (len > 12) len = 12;
        for (int i = 0; i < len; i++) {
            padded[i] = headers[c][i];
        }

        for (int i = 0; i < 12; i++) {
            temp[cursor++] = padded[i];
        }

        if (c < MAX_COLS - 1) {
            temp[cursor++] = ' ';
            temp[cursor++] = '|';
            temp[cursor++] = ' ';
        }
    }
    temp[cursor] = '\0';
    str_strncpy(dest, temp, max_len);
}

static void rewrite_db_file(const char* db_file, db_row_t* rows, int row_count) {
    char new_content[4096];
    memset(new_content, 0, sizeof(new_content));

    char header_line[128];
    memset(header_line, 0, sizeof(header_line));
    for (int c = 0; c < MAX_COLS; c++) {
        str_strcat(header_line, headers[c]);
        if (c < MAX_COLS - 1) str_strcat(header_line, ",");
    }
    str_strcat(header_line, "\n");
    str_strcpy(new_content, header_line);

    for (int i = 0; i < row_count; i++) {
        char line_buf[128];
        memset(line_buf, 0, sizeof(line_buf));
        for (int c = 0; c < MAX_COLS; c++) {
            str_strcat(line_buf, rows[i].fields[c]);
            if (c < MAX_COLS - 1) str_strcat(line_buf, ",");
        }
        str_strcat(line_buf, "\n");
        str_strcat(new_content, line_buf);
    }

    delete_file((char*)db_file);
    if (new_content[0] != '\0') {
        create_file((char*)db_file);
        append_file(db_file, (const uint8_t*)new_content, str_strlen(new_content));
    }
}

void _start(void) {
    char db_file[PATH_CAPACITY];
    memset(db_file, 0, sizeof(db_file));

    background_color(COLOR_BACKGROUND);
    text_color(COLOR_WHITE);
    clear();

    print_at("=== Doge Databaser v1.0 ===", 2, 1, COLOR_BRIGHT_ORANGE);
    print_at("---------------------------------------------------------", 2, 2, COLOR_DARK_GRAY);
    
    input("\n\n  Enter database file path (.csv required): ", db_file, sizeof(db_file) - 1);

    if (!ends_with_csv(db_file) || !file_exists(db_file)) {
        clear();
        print_at("=== Doge Databaser v1.0 ===", 2, 1, COLOR_BRIGHT_ORANGE);
        print_at("---------------------------------------------------------", 2, 2, COLOR_DARK_GRAY);
        
        if (!ends_with_csv(db_file)) {
            print_at("[ERROR] Invalid file format or missing .csv extension!", 2, 4, COLOR_BRIGHT_RED);
        } else {
            print_at("File not found: ", 2, 4, COLOR_WHITE);
            print_at(db_file, 18, 4, COLOR_BRIGHT_CYAN);
        }

        print_at("[N] Make a new .csv file  |  [Q] Exit", 2, 6, COLOR_BRIGHT_GREEN);

        while (1) {
            uint64_t key = get_key();
            if (key == 'q' || key == 'Q') {
                clear();
                sys_exit(0);
            } else if (key == 'n' || key == 'N') {
                clear();
                print_at("=== Doge Databaser v1.0 - Schema Setup ===", 2, 1, COLOR_BRIGHT_ORANGE);
                print_at("---------------------------------------------------------", 2, 2, COLOR_DARK_GRAY);
                
                char new_path[PATH_CAPACITY];
                memset(new_path, 0, sizeof(new_path));
                input("\n\n  Enter new .csv filename: ", new_path, sizeof(new_path) - 1);

                if (!ends_with_csv(new_path)) {
                    clear();
                    print_at("[ERROR] File must end in .csv. Exiting...", 2, 3, COLOR_BRIGHT_RED);
                    sys_exit(0);
                }
                str_strcpy(db_file, new_path);

                for (int c = 0; c < MAX_COLS; c++) {
                    char prompt_buf[40];
                    memset(prompt_buf, 0, sizeof(prompt_buf));
                    str_strcpy(prompt_buf, "  Column ");
                    prompt_buf[9] = (char)('1' + c);
                    str_strcat(prompt_buf, " Name: ");

                    memset(headers[c], 0, FIELD_LEN);
                    input(prompt_buf, headers[c], FIELD_LEN - 1);
                    
                    if (headers[c][0] == '\0') {
                        if (c == 0) str_strcpy(headers[c], "NAME");
                        else if (c == 1) str_strcpy(headers[c], "PHONE");
                        else if (c == 2) str_strcpy(headers[c], "AGE");
                        else if (c == 3) str_strcpy(headers[c], "BALANCE");
                        else if (c == 4) str_strcpy(headers[c], "STATUS");
                    }
                }

                create_file(db_file);
                char header_line[128];
                memset(header_line, 0, sizeof(header_line));
                for (int c = 0; c < MAX_COLS; c++) {
                    str_strcat(header_line, headers[c]);
                    if (c < MAX_COLS - 1) str_strcat(header_line, ",");
                }
                str_strcat(header_line, "\n");
                append_file(db_file, (const uint8_t*)header_line, str_strlen(header_line));
                break;
            }
        }
    }

    int selected = 0;
    int needs_redraw = 1;

    while (1) {
        db_row_t rows[MAX_RECORDS];
        int row_count = 0;
        char file_buf[4096];
        memset(file_buf, 0, sizeof(file_buf));
        int bytes = (int)read_file((char*)db_file, file_buf, sizeof(file_buf) - 1);

        int line_idx = 0;
        if (bytes > 0) {
            file_buf[bytes] = '\0';
            char* line_start = file_buf;
            for (int i = 0; i <= bytes; i++) {
                if (file_buf[i] == '\n' || file_buf[i] == '\0') {
                    file_buf[i] = '\0';
                    if (line_start[0] != '\0') {
                        if (line_idx == 0) {
                            int h_col = 0;
                            char* hp = line_start;
                            int h_idx = 0;
                            memset(headers, 0, sizeof(headers));
                            while (*hp != '\0' && h_col < MAX_COLS) {
                                if (*hp == ',') {
                                    headers[h_col][h_idx] = '\0';
                                    if (headers[h_col][0] == '\0') {
                                        if (h_col == 0) str_strcpy(headers[h_col], "NAME");
                                        else if (h_col == 1) str_strcpy(headers[h_col], "PHONE");
                                        else if (h_col == 2) str_strcpy(headers[h_col], "AGE");
                                        else if (h_col == 3) str_strcpy(headers[h_col], "BALANCE");
                                        else if (h_col == 4) str_strcpy(headers[h_col], "STATUS");
                                    }
                                    h_col++;
                                    h_idx = 0;
                                } else {
                                    if (h_idx < FIELD_LEN - 1) {
                                        headers[h_col][h_idx++] = *hp;
                                    }
                                }
                                hp++;
                            }
                            if (h_col < MAX_COLS) {
                                headers[h_col][h_idx] = '\0';
                                if (headers[h_col][0] == '\0') {
                                    if (h_col == 0) str_strcpy(headers[h_col], "NAME");
                                    else if (h_col == 1) str_strcpy(headers[h_col], "PHONE");
                                    else if (h_col == 2) str_strcpy(headers[h_col], "AGE");
                                    else if (h_col == 3) str_strcpy(headers[h_col], "BALANCE");
                                    else if (h_col == 4) str_strcpy(headers[h_col], "STATUS");
                                }
                            }
                        } else {
                            if (parse_csv_line(line_start, &rows[row_count]) == 0) {
                                row_count++;
                                if (row_count >= MAX_RECORDS) break;
                            }
                        }
                        line_idx++;
                    }
                    line_start = &file_buf[i + 1];
                }
            }
        }

        if (selected >= row_count && row_count > 0) {
            selected = row_count - 1;
            needs_redraw = 1;
        }
        if (selected < 0) {
            selected = 0;
        }

        if (needs_redraw) {
            clear();
            
            print_at("[Doge Databaser v1.0 - File: ", 2, 1, COLOR_BRIGHT_ORANGE);
            print_at(db_file, 29, 1, COLOR_BRIGHT_CYAN);
            print_at("]", 29 + (uint32_t)str_strlen(db_file), 1, COLOR_BRIGHT_ORANGE);
            print_at("====================================================================", 2, 2, COLOR_DARK_GRAY);
            
            char formatted_headers[128];
            serialize_headers(formatted_headers, sizeof(formatted_headers));
            print_at(formatted_headers, 3, 3, COLOR_BRIGHT_YELLOW);

            print_at("====================================================================", 2, 4, COLOR_DARK_GRAY);

            if (row_count == 0) {
                print_at("[ No Records Found - Press 'N' to Add ]", 4, 6, COLOR_BRIGHT_BLACK);
            } else {
                for (int i = 0; i < row_count; i++) {
                    uint32_t y = 5 + (uint32_t)i;
                    if (y > 15) break;

                    char formatted_row[128];
                    serialize_row(&rows[i], formatted_row, sizeof(formatted_row));

                    if (i == selected) {
                        print_at(">", 1, y, COLOR_CURSOR);
                        print_at(formatted_row, 3, y, COLOR_BRIGHT_CYAN);
                    } else {
                        print_at(" ", 1, y, COLOR_BACKGROUND);
                        print_at(formatted_row, 3, y, COLOR_WHITE);
                    }
                }
            }

            print_at("====================================================================", 2, 16, COLOR_DARK_GRAY);
            print_at("[W/S/Up/Down] Nav | [N] New | [D] Del | [C] Change Labels | [Q] Exit", 2, 17, COLOR_BRIGHT_GREEN);
            
            needs_redraw = 0;
        }

        uint64_t key = get_key();
        if (key == 0) {
            continue;
        }

        if (key == 'w' || key == 'W' || key == KEY_UP) {
            if (selected > 0) {
                selected--;
                needs_redraw = 1;
            }
        } 
        else if (key == 's' || key == 'S' || key == KEY_DOWN) {
            if (selected < row_count - 1) {
                selected++;
                needs_redraw = 1;
            }
        } 
        else if (key == 'n' || key == 'N') {
            print_at("                                                         ", 2, 18, COLOR_BACKGROUND);
            print_at("                                                         ", 2, 19, COLOR_BACKGROUND);
            
            char field_bufs[MAX_COLS][32];
            print("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
            for (int c = 0; c < MAX_COLS; c++) {
                memset(field_bufs[c], 0, sizeof(field_bufs[c]));
                char prompt_buf[40];
                memset(prompt_buf, 0, sizeof(prompt_buf));
                str_strcpy(prompt_buf, "Enter ");
                str_strcat(prompt_buf, headers[c]);
                str_strcat(prompt_buf, ": ");
                
                text_color(COLOR_GRAY);
                print("  ");
                input(prompt_buf, field_bufs[c], sizeof(field_bufs[c]) - 1);
            }

            if (row_count < MAX_RECORDS) {
                for (int c = 0; c < MAX_COLS; c++) {
                    str_strncpy(rows[row_count].fields[c], field_bufs[c], FIELD_LEN - 1);
                }
                row_count++;
                rewrite_db_file(db_file, rows, row_count);
            }
            needs_redraw = 1;
        }
        else if (key == 'c' || key == 'C') {
            print_at("                                                         ", 2, 18, COLOR_BACKGROUND);
            print_at("                                                         ", 2, 19, COLOR_BACKGROUND);
            print_at("--- Configuring Column Labels ---                        ", 2, 18, COLOR_BRIGHT_MAGENTA);

            print("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
            for (int c = 0; c < MAX_COLS; c++) {
                char prompt_buf[40];
                memset(prompt_buf, 0, sizeof(prompt_buf));
                str_strcpy(prompt_buf, "New Header ");
                prompt_buf[11] = (char)('1' + c);
                str_strcat(prompt_buf, ": ");

                char new_lbl[FIELD_LEN];
                memset(new_lbl, 0, sizeof(new_lbl));
                print("  ");
                input(prompt_buf, new_lbl, FIELD_LEN - 1);
                if (new_lbl[0] != '\0') {
                    str_strncpy(headers[c], new_lbl, FIELD_LEN - 1);
                }
            }

            rewrite_db_file(db_file, rows, row_count);
            needs_redraw = 1;
        }
        else if (key == 'd' || key == 'D') {
            if (row_count > 0) {
                for (int i = selected; i < row_count - 1; i++) {
                    rows[i] = rows[i + 1];
                }
                row_count--;
                rewrite_db_file(db_file, rows, row_count);
                if (selected > 0 && selected >= row_count) selected--;
                needs_redraw = 1;
            }
        }
        else if (key == 'q' || key == 'Q') {
            clear();
            sys_exit(0);
        }
    }
}