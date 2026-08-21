#include <string.h>
#include <bool.h>
#include <system.h>
#include <dogeio.h>

#define EDIT_BUF_SIZE 8192

static char editor_buf[EDIT_BUF_SIZE];
static size_t editor_len = 0;
static bool is_modified = false;

static void trim_input(char* str) {
    if (!str) return;
    size_t len = str_strlen(str);
    while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

static int count_lines(const char* buf, size_t len) {
    int lines = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n') lines++;
    }
    return lines;
}

static void delete_last_line_in_ram(void) {
    if (editor_len == 0) return;

    size_t idx = editor_len;
    if (idx > 0 && editor_buf[idx - 1] == '\n') {
        idx--;
    }

    while (idx > 0 && editor_buf[idx - 1] != '\n') {
        idx--;
    }

    editor_buf[idx] = '\0';
    editor_len = idx;
}

static bool save_to_disk(const char* filename) {
    if (fs_exists((char*)filename)) {
        fs_delete((char*)filename);
    }

    if (fs_create((char*)filename) < 0) {
        return false;
    }

    if (editor_len > 0) {
        editor_buf[editor_len] = '\0';
        if (fs_write((char*)filename, editor_buf) < 0) {
            return false;
        }
    }

    is_modified = false;
    return true;
}

void system_editor(char* filename) {
    if (!filename || filename[0] == '\0') {
        dogeio_text_println("Error: Invalid filename.");
        return;
    }

    memset(editor_buf, 0, sizeof(editor_buf));
    editor_len = 0;
    is_modified = false;

    if (fs_exists(filename)) {
        int bytes = fs_read(filename, editor_buf, sizeof(editor_buf) - 1);
        if (bytes > 0) {
            editor_len = (size_t)bytes;
            editor_buf[editor_len] = '\0';
        }
    }

    dogeio_text_clear();
    dogeio_text_println("=== DogeEdit v2.0 ===");
    dogeio_text_println("i : Insert Text  | o : Show Output | d : Delete Line");
    dogeio_text_println("w : Quick Save   | s : File Status | c : Clear Screen");
    dogeio_text_println("e : Save & Exit  | q : Quit (Discard Changes)");
    dogeio_text_println("Your computer may explode, make sure to beat tidal wave too.");
    dogeio_text_println("====================================");

    char cmd[16];
    char input_line[512];
    char num_str[16];

    while (true) {
        dogeio_text_print(is_modified ? "[*] > " : "> ");
        dogeio_text_input("", cmd, sizeof(cmd) - 1);
        trim_input(cmd);

        if (str_strcmp(cmd, "i") == 0) {
            dogeio_text_println("Insert Mode. Type '.' on a line by itself to exit.");
            while (true) {
                int line_num = count_lines(editor_buf, editor_len) + 1;
                str_itoa(line_num, num_str);
                dogeio_text_print(num_str);

                dogeio_text_input(": ", input_line, sizeof(input_line) - 1);
                trim_input(input_line);

                if (str_strcmp(input_line, ".") == 0) break;

                size_t line_len = str_strlen(input_line);
                if (editor_len + line_len + 1 >= sizeof(editor_buf) - 1) {
                    dogeio_text_println("Error: Editor RAM buffer full!");
                    break;
                }

                for (size_t i = 0; i < line_len; i++) {
                    editor_buf[editor_len++] = input_line[i];
                }
                editor_buf[editor_len++] = '\n';
                editor_buf[editor_len] = '\0';
                is_modified = true;
            }
            dogeio_text_println("Exited Insert Mode.");

        } else if (str_strcmp(cmd, "o") == 0) {
            if (editor_len == 0) {
                dogeio_text_println("(Buffer empty)");
            } else {
                dogeio_text_println("--- Buffer Content ---");
                dogeio_text_print(editor_buf);
                dogeio_text_println("----------------------");
            }

        } else if (str_strcmp(cmd, "d") == 0) {
            if (editor_len == 0) {
                dogeio_text_println("Buffer is empty.");
            } else {
                delete_last_line_in_ram();
                is_modified = true;
                dogeio_text_println("Last line removed.");
            }

        } else if (str_strcmp(cmd, "w") == 0) {
            if (save_to_disk(filename)) {
                dogeio_text_println("Saved changes to disk.");
            } else {
                dogeio_text_println("Error: Failed writing to disk.");
            }

        } else if (str_strcmp(cmd, "s") == 0) {
            dogeio_text_print("File: ");
            dogeio_text_println(filename);

            str_itoa((int)editor_len, num_str);
            dogeio_text_print("Size: ");
            dogeio_text_print(num_str);
            dogeio_text_println(" bytes");

            str_itoa(count_lines(editor_buf, editor_len), num_str);
            dogeio_text_print("Lines: ");
            dogeio_text_println(num_str);

            dogeio_text_print("Status: ");
            dogeio_text_println(is_modified ? "Modified (unsaved)" : "Saved");

        } else if (str_strcmp(cmd, "c") == 0) {
            dogeio_text_clear();
            dogeio_text_println("i: Insert | o: Output | d: Del Line | w: Save | s: Status | e: Save&Exit | q: Quit");

        } else if (str_strcmp(cmd, "e") == 0) {
            if (save_to_disk(filename)) {
                dogeio_text_println("File saved. Exiting DogeEdit.");
            } else {
                dogeio_text_println("Error saving file on exit!");
            }
            return;

        } else if (str_strcmp(cmd, "q") == 0) {
            dogeio_text_println("Exited without saving.");
            return;

        } else {
            dogeio_text_println("Unknown command. Type 'c' for options.");
        }
    }
}