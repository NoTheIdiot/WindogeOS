#include <boot/kernel.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <system.h>
#include <core.h>
#include <basicutil.h>
#include <image.h>
#include <bool.h>

typedef struct {
    const char *label;
    const char *value;
    uint32_t color;
} settings_color_t;

static const settings_color_t settings_colors[] = {
    {"Gray", "gray", 0xFFCCCCCC},
    {"White", "white", 0xFFFFFFFF},
    {"Red", "red", 0xFFFF5555},
    {"Green", "green", 0xFF55FF55},
    {"Yellow", "yellow", 0xFFFFED29},
    {"Blue", "blue", 0xFF5555FF},
    {"Magenta", "magenta", 0xFFFF55FF},
    {"Cyan", "cyan", 0xFF55FFFF},
};

static char settings_path[] = "/.windoge";
static size_t selected_color;
static int default_shell_is_bash;

uint32_t saved_color = 0xFFCCCCCC;

static void settings_draw_menu(const char *title, const char *const *items,
                               size_t item_count, size_t selected) {
    dogeio_text_clear();
    dogeio_text_println(title);
    dogeio_text_println("");
    for (size_t i = 0; i < item_count; i++) {
        dogeio_text_print(i == selected ? "  > " : "    ");
        dogeio_text_println(items[i]);
    }
    dogeio_text_println("");
    dogeio_text_println("Use Up/Down and Enter. Backspace returns.");
}

static uint16_t settings_wait_key(void) {
    uint16_t key;
    do {
        key = dogeio_get_key();
    } while (key == KEY_UNKNOWN);
    return key;
}

static void settings_load_value(const char *value) {
    for (size_t i = 0; i < sizeof(settings_colors) / sizeof(settings_colors[0]); i++) {
        if (str_strcmp(value, settings_colors[i].value) == 0) {
            selected_color = i;
            saved_color = settings_colors[i].color;
            return;
        }
    }
}

void system_load_settings(void) {
    char contents[128];
    int bytes_read;

    selected_color = 0;
    saved_color = settings_colors[0].color;
    default_shell_is_bash = 0;

    if (!fs_exists(settings_path)) {
        return;
    }

    bytes_read = fs_read(settings_path, contents, sizeof(contents) - 1);
    if (bytes_read < 0) {
        dogeio_text_println("Settings error: unable to read /.windoge.");
        dogeio_text_color_change(saved_color);
        return;
    }
    contents[bytes_read] = '\0';

    char *line = contents;
    while (*line != '\0') {
        char *line_end = line;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }
        char line_buffer[32];
        size_t line_length = (size_t)(line_end - line);
        if (line_length < sizeof(line_buffer)) {
            str_strncpy(line_buffer, line, line_length + 1);
            line_buffer[line_length] = '\0';
            if (str_startswith(line_buffer, "text_color=")) {
                settings_load_value(line_buffer + 11);
            } else if (str_strcmp(line_buffer, "shell=bash") == 0) {
                default_shell_is_bash = 1;
            } else if (str_strcmp(line_buffer, "shell=dogeshell") == 0) {
                default_shell_is_bash = 0;
            }
        }
        line = line_end;
        while (*line == '\n' || *line == '\r') {
            line++;
        }
    }
    dogeio_text_color_change(saved_color);
}

static int settings_save(void) {
    char contents[64] = "text_color=";
    str_strcat(contents, settings_colors[selected_color].value);
    str_strcat(contents, "\nshell=");
    str_strcat(contents, default_shell_is_bash ? "bash\n" : "dogeshell\n");

    if (!fs_exists(settings_path) &&
        fs_create(settings_path) != 0) {
        return -1;
    }
    return fs_write(settings_path, contents);
}

static void settings_show_system_information(void) {
    char ram_buffer[32];
    dogeio_text_clear();
    dogeio_text_println("[ System Information ]");
    dogeio_text_print("[Version]> ");
    dogeio_text_println(windoge_version);
    dogeio_text_print("[CPU]> ");
    dogeio_text_println(cpuid());
    dogeio_text_print("[RAM]> ");
    dogeio_text_print(uint64_to_str(get_ram() / 1024 / 1024, ram_buffer));
    dogeio_text_println(" MB");
    dogeio_text_println("");
    dogeio_text_println("Press any key to return.");
    settings_wait_key();
}

static void settings_draw_color_menu(size_t selected) {
    size_t color_count = sizeof(settings_colors) / sizeof(settings_colors[0]);
    dogeio_text_clear();
    dogeio_text_println("[ Default Text Color ]");
    dogeio_text_println("");
    for (size_t i = 0; i < color_count; i++) {
        dogeio_text_print(i == selected ? "  > " : "    ");
        dogeio_text_println(settings_colors[i].label);
    }
    dogeio_text_println("");
    dogeio_text_println("Use Up/Down and Enter. Backspace returns.");
}

static void settings_select_color(void) {
    size_t selected = selected_color;
    size_t color_count = sizeof(settings_colors) / sizeof(settings_colors[0]);

    while (true) {
        settings_draw_color_menu(selected);
        uint16_t key = settings_wait_key();
        if (key == KEY_UP) {
            selected = selected == 0 ? color_count - 1 : selected - 1;
        } else if (key == KEY_DOWN) {
            selected = (selected + 1) % color_count;
        } else if (key == KEY_BACKSPACE || key == (uint16_t)'q') {
            return;
        } else if (key == KEY_ENTER) {
            selected_color = selected;
            saved_color = settings_colors[selected_color].color;
            dogeio_text_color_change(saved_color);
            if (settings_save() != 0) {
                dogeio_text_println("Unable to save settings to /.windoge.");
            } else {
                dogeio_text_println("Default text color saved.");
            }
            settings_wait_key();
            return;
        }
    }
}

static void settings_select_shell(void) {
    static const char *shell_labels[] = {"Dogeshell", "Bash"};
    size_t selected = default_shell_is_bash ? 1 : 0;

    while (true) {
        settings_draw_menu("[ Default Shell ]", shell_labels,
                           sizeof(shell_labels) / sizeof(shell_labels[0]), selected);
        uint16_t key = settings_wait_key();
        if (key == KEY_UP || key == KEY_DOWN) {
            selected = selected == 0 ? 1 : 0;
        } else if (key == KEY_BACKSPACE || key == (uint16_t)'q') {
            return;
        } else if (key == KEY_ENTER) {
            default_shell_is_bash = selected == 1;
            if (settings_save() != 0) {
                dogeio_text_println("Unable to save settings to /.windoge.");
            } else {
                dogeio_text_println("Default shell saved. It will be used after reboot.");
            }
            settings_wait_key();
            return;
        }
    }
}

static void settings_run_test(void) {
    char* save = fs_dirname();
    fs_chdir("/");
    system_dogeshell_ex("run /apps/syscall_test.bin");
    fs_chdir(save);
}

void system_settings(void) {
    static const char *menu_items[] = {
        "System Information",
        "Default Text Color",
        "Default Shell",
        "Test System",
        "Exit",
    };
    const size_t menu_count = sizeof(menu_items) / sizeof(menu_items[0]);
    size_t selected = 0;

    while (true) {
        settings_draw_menu("[ WindogeOS Settings ]", menu_items, menu_count, selected);
        uint16_t key = settings_wait_key();
        if (key == KEY_UP) {
            selected = selected == 0 ? menu_count - 1 : selected - 1;
        } else if (key == KEY_DOWN) {
            selected = (selected + 1) % menu_count;
        } else if (key == KEY_BACKSPACE || key == (uint16_t)'q') {
            break;
        } else if (key == KEY_ENTER) {
            if (selected == 0) {
                settings_show_system_information();
            } else if (selected == 1) {
                settings_select_color();
            } else if (selected == 2) {
                settings_select_shell();
            } else if (selected == 3) {
                settings_run_test();
            } else {
                break;
            }
        }
    }
    dogeio_text_clear();
    dogeio_text_color_change(saved_color);
}

void system_start_default_shell(void) {
    if (default_shell_is_bash) {
        system_bash();
    } else {
        system_dogeshell();
    }
}

static int dogeshell_exit_requested;
static int dogeshell_last_status;
static char dogeshell_old_directory[256];

#define DOGESHELL_LINE_SIZE 256
#define DOGESHELL_ARGUMENT_COUNT 24

static size_t dogeshell_strlen(const char *text) {
    size_t length = 0;
    if (text != NULL) {
        while (text[length] != '\0') {
            length++;
        }
    }
    return length;
}

static int dogeshell_copy(char *destination, size_t capacity,
                          const char *source) {
    size_t length = dogeshell_strlen(source);
    if (destination == NULL || capacity == 0 || length >= capacity) {
        return -1;
    }
    for (size_t i = 0; i <= length; i++) {
        destination[i] = source[i];
    }
    return 0;
}

static int dogeshell_append(char *destination, size_t capacity, size_t *length,
                            const char *source) {
    size_t source_length = dogeshell_strlen(source);
    if (*length >= capacity || source_length >= capacity - *length) {
        return -1;
    }
    for (size_t i = 0; i < source_length; i++) {
        destination[*length + i] = source[i];
    }
    *length += source_length;
    destination[*length] = '\0';
    return 0;
}

static int dogeshell_tokenize(const char *line, char *storage,
                              size_t storage_size, char **arguments,
                              size_t argument_capacity) {
    size_t read_index = 0;
    size_t write_index = 0;
    size_t argument_count = 0;

    while (line[read_index] != '\0') {
        char quote = '\0';
        size_t argument_start;
        while (line[read_index] == ' ' || line[read_index] == '\t') {
            read_index++;
        }
        if (line[read_index] == '\0') {
            break;
        }
        if (argument_count == argument_capacity || write_index >= storage_size) {
            return -1;
        }

        argument_start = write_index;
        arguments[argument_count++] = storage + argument_start;

        while (line[read_index] != '\0') {
            char character = line[read_index];
            if (quote == '\0' && (character == ' ' || character == '\t')) {
                break;
            }
            if (character == '\\' && quote != '\'') {
                read_index++;
                if (line[read_index] == '\0') {
                    return -1;
                }
                character = line[read_index++];
            } else if (quote != '\0') {
                if (character == quote) {
                    quote = '\0';
                    read_index++;
                    continue;
                }
                read_index++;
            } else if (character == '\'' || character == '"') {
                quote = character;
                read_index++;
                continue;
            } else {
                read_index++;
            }

            if (write_index + 1 >= storage_size) {
                return -1;
            }
            storage[write_index++] = character;
        }
        if (quote != '\0' || write_index >= storage_size) {
            return -1;
        }
        storage[write_index++] = '\0';
    }

    return (int)argument_count;
}

static int dogeshell_build_home(char *path, size_t capacity) {
    size_t length = 0;
    path[0] = '\0';
    return dogeshell_append(path, capacity, &length, "/users/") == 0 &&
                   dogeshell_append(path, capacity, &length, current_user) == 0
               ? 0
               : -1;
}

static int dogeshell_resolve_path(const char *input, char *output,
                                  size_t output_capacity) {
    char combined[512] = {0};
    const char *working_directory = fs_dirname();
    size_t combined_length = 0;
    size_t output_length = 1;
    size_t index = 0;

    if (input == NULL || input[0] == '\0' || output_capacity < 2) {
        return -1;
    }

    if (input[0] == '~' && (input[1] == '\0' || input[1] == '/')) {
        if (dogeshell_build_home(combined, sizeof(combined)) != 0) {
            return -1;
        }
        combined_length = dogeshell_strlen(combined);
        input++;
    } else if (input[0] != '/') {
        if (working_directory == NULL ||
            dogeshell_append(combined, sizeof(combined), &combined_length,
                             working_directory) != 0 ||
            (combined_length > 0 && combined[combined_length - 1] != '/' &&
             dogeshell_append(combined, sizeof(combined), &combined_length,
                              "/") != 0)) {
            return -1;
        }
    }
    if (dogeshell_append(combined, sizeof(combined), &combined_length, input) != 0) {
        return -1;
    }

    output[0] = '/';
    output[1] = '\0';
    while (combined[index] != '\0') {
        size_t segment_start;
        size_t segment_length;
        size_t separator_length;

        while (combined[index] == '/') {
            index++;
        }
        if (combined[index] == '\0') {
            break;
        }
        segment_start = index;
        while (combined[index] != '\0' && combined[index] != '/') {
            index++;
        }
        segment_length = index - segment_start;
        if (segment_length == 1 && combined[segment_start] == '.') {
            continue;
        }
        if (segment_length == 2 && combined[segment_start] == '.' &&
            combined[segment_start + 1] == '.') {
            while (output_length > 1 && output[output_length - 1] != '/') {
                output_length--;
            }
            if (output_length > 1) {
                output_length--;
            }
            output[output_length] = '\0';
            continue;
        }

        separator_length = output_length > 1 ? 1 : 0;
        if (output_length + separator_length + segment_length >= output_capacity) {
            return -1;
        }
        if (separator_length != 0) {
            output[output_length++] = '/';
        }
        for (size_t i = 0; i < segment_length; i++) {
            output[output_length++] = combined[segment_start + i];
        }
        output[output_length] = '\0';
    }
    return 0;
}

static int dogeshell_authorize_path(const char *path, char *resolved,
                                    size_t resolved_size) {
    if (dogeshell_resolve_path(path, resolved, resolved_size) != 0) {
        dogeio_text_println("Error: invalid or overlong path.");
        return 0;
    }
    if (!system_can_access_path(current_user, resolved)) {
        dogeio_text_println("Error: permission denied.");
        return 0;
    }
    return 1;
}

static int dogeshell_history_path(char *path, size_t capacity) {
    size_t length = 0;
    path[0] = '\0';
    return dogeshell_append(path, capacity, &length, "/users/") == 0 &&
                   dogeshell_append(path, capacity, &length, current_user) == 0 &&
                   dogeshell_append(path, capacity, &length, "/.history") == 0
               ? 0
               : -1;
}

static int dogeshell_append_history(const char *command) {
    char path[160];
    char line[DOGESHELL_LINE_SIZE] = {0};
    if (command == NULL) {
        return 0;
    }
    size_t length = dogeshell_strlen(command);

    if (length == 0) {
        return 0;
    }
    if (length + 1 >= sizeof(line) ||
        dogeshell_history_path(path, sizeof(path)) != 0) {
        return -1;
    }
    if (!fs_exists(path) && fs_create(path) != 0) {
        return -1;
    }
    for (size_t i = 0; i < length; i++) {
        line[i] = command[i];
    }
    line[length++] = '\n';
    return fs_append_data(path, (const uint8_t *)line, (uint32_t)length);
}

static void dogeshell_print_help(void) {
    static const char *const help[] = {
        "Basic Functions",
        "============================================================",
        "  print/echo <text>        | print text",
        "  clear                    | clear the terminal",
        "  ver                      | show shell version",
        "  help                     | show this help",
        "  history                  | show command history",
        "  clear-history            | clear command history",
        "  time, date               | show the current time or date",
        "  shutdown, reboot         | power off or restart the system",
        "============================================================",
        "",
        "File System",
        "============================================================",
        "  dir [path] [--hidden]    | list a directory",
        "  read <file>              | print file contents",
        "  write <file>             | replace a file with one line of text",
        "  create <file>            | create an empty file",
        "  mkdir <directory>        | create a directory",
        "  del <file>               | delete a file",
        "  rename <old> <new>       | rename a file",
        "  whereami                 | print the current directory",
        "  cd [directory]           | change directory",
        "============================================================",
        "",
        "System and Utilities",
        "============================================================",
        "  whoami, cpuinfo, raminfo | show user and system information",
        "  fetch, settings, tab     | system UI and terminal controls",
        "  edit <file>              | open the text editor",
        "  pci                      | list PCI devices",
        "  calc <expression>        | evaluate a calculator expression",
        "  hexdump <file>           | display file bytes",
        "  run <file>               | run a flat binary",
        "  <app>[.bin]              | run an app from /apps",
        "  bash                     | open Bash; exit returns here",
        "  exit                     | leave Dogeshell",
        "============================================================",
        "",
        "Paths may be quoted; use ~ for your home directory."
    };
    for (size_t i = 0; i < sizeof(help) / sizeof(help[0]); i++) {
        dogeio_text_println(help[i]);
    }
}

static int dogeshell_show_history(void) {
    static char contents[8192];
    char path[160];
    int bytes_read;

    if (dogeshell_history_path(path, sizeof(path)) != 0) {
        dogeio_text_println("History error: path is too long.");
        return 1;
    }
    bytes_read = fs_read(path, contents, sizeof(contents) - 1);
    if (bytes_read < 0) {
        dogeio_text_println("No history available.");
        return 1;
    }
    if (bytes_read == 0) {
        dogeio_text_println("No history available.");
        return 0;
    }
    if (bytes_read >= (int)sizeof(contents)) {
        dogeio_text_println("History error: invalid file size.");
        return 1;
    }
    contents[bytes_read] = '\0';
    dogeio_text_print(contents);
    if (contents[bytes_read - 1] != '\n') {
        dogeio_text_println("");
    }
    return 0;
}

static int dogeshell_print_file(const char *path) {
    static char contents[8192];
    int bytes_read = fs_read((char *)path, contents, sizeof(contents) - 1);
    if (bytes_read < 0 || bytes_read >= (int)sizeof(contents)) {
        dogeio_text_println("Unable to read file.");
        return 1;
    }
    contents[bytes_read] = '\0';
    dogeio_text_print(contents);
    if (bytes_read == 0 || contents[bytes_read - 1] != '\n') {
        dogeio_text_println("");
    }
    return 0;
}

static int dogeshell_join_arguments(int argc, char **argv, int first,
                                    char *output, size_t capacity) {
    size_t length = 0;
    output[0] = '\0';
    for (int i = first; i < argc; i++) {
        if ((i != first &&
             dogeshell_append(output, capacity, &length, " ") != 0) ||
            dogeshell_append(output, capacity, &length, argv[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int dogeshell_find_app(const char *command, char *app_path,
                              size_t app_path_capacity) {
    char filename[128];
    char path[160];
    size_t name_length = dogeshell_strlen(command);
    size_t path_length = 0;
    int has_extension = 0;

    if (name_length == 0 || name_length >= sizeof(filename)) {
        return 1;
    }
    for (size_t i = 0; i < name_length; i++) {
        if (command[i] == '/' || command[i] == '\\') {
            return 1;
        }
    }
    if (name_length >= 4) {
        const char *extension = command + name_length - 4;
        has_extension = extension[0] == '.' &&
                        (extension[1] == 'b' || extension[1] == 'B') &&
                        (extension[2] == 'i' || extension[2] == 'I') &&
                        (extension[3] == 'n' || extension[3] == 'N');
    }
    if (dogeshell_copy(filename, sizeof(filename), command) != 0) {
        return 1;
    }
    if (!has_extension &&
        dogeshell_append(filename, sizeof(filename), &name_length, ".bin") != 0) {
        return 1;
    }
    if (dogeshell_append(path, sizeof(path), &path_length, "/apps/") != 0 ||
        dogeshell_append(path, sizeof(path), &path_length, filename) != 0 ||
        !fs_exists(path)) {
        return 1;
    }
    if (dogeshell_copy(app_path, app_path_capacity, path) != 0) {
        return 1;
    }
    return 0;
}

static int dogeshell_list_pci(void) {
    dogeio_text_println("ADDR      IDENTITY DESCRIPTION & [VENDOR:DEVICE ID]");
    dogeio_text_println("---------------------------------------------------------");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t function = 0; function < 8; function++) {
                uint16_t vendor = pci_read_16((uint8_t)bus, slot, function, 0x00);
                if (vendor == 0xFFFF) {
                    if (function == 0) {
                        break;
                    }
                    continue;
                }
                uint16_t device = pci_read_16((uint8_t)bus, slot, function, 0x02);
                uint8_t class_code = pci_read_8((uint8_t)bus, slot, function, 0x0B);
                char function_text[2] = {(char)('0' + function), '\0'};
                dogeio_print_hex8((uint8_t)bus);
                dogeio_text_print(":");
                dogeio_print_hex8(slot);
                dogeio_text_print(".");
                dogeio_text_print(function_text);
                dogeio_text_print("    ");
                dogeio_text_print(pci_class_to_name(class_code));
                dogeio_text_print(" [");
                dogeio_print_hex16(vendor);
                dogeio_text_print(":");
                dogeio_print_hex16(device);
                dogeio_text_println("]");
            }
        }
    }
    return 0;
}

static int dogeshell_execute(int argc, char **argv) {
    char path[256];

    if (argc == 0) {
        return 0;
    }
    if (str_strcmp(argv[0], "exit") == 0) {
        dogeshell_exit_requested = 1;
        return dogeshell_last_status;
    }
    if (str_strcmp(argv[0], "help") == 0) {
        dogeshell_print_help();
        return 0;
    }
    if (str_strcmp(argv[0], "print") == 0 || str_strcmp(argv[0], "echo") == 0) {
        char output[DOGESHELL_LINE_SIZE];
        if (dogeshell_join_arguments(argc, argv, 1, output, sizeof(output)) != 0) {
            dogeio_text_println("Error: text is too long.");
            return 1;
        }
        dogeio_text_println(output);
        return 0;
    }
    if (str_strcmp(argv[0], "clear") == 0) {
        dogeio_text_clear();
        return 0;
    }
    if (str_strcmp(argv[0], "ver") == 0) {
        dogeio_text_println(dogeshell_version);
        return 0;
    }
    if (str_strcmp(argv[0], "history") == 0) {
        return dogeshell_show_history();
    }
    if (str_strcmp(argv[0], "clear-history") == 0) {
        char history_path[160];
        if (dogeshell_history_path(history_path, sizeof(history_path)) != 0) {
            dogeio_text_println("History error: path is too long.");
            return 1;
        }
        if (fs_exists(history_path) && fs_delete(history_path) != 1) {
            dogeio_text_println("History error: unable to delete history.");
            return 1;
        }
        if (fs_create(history_path) != 0) {
            dogeio_text_println("History error: unable to create history.");
            return 1;
        }
        dogeio_text_println("History cleared.");
        return 0;
    }
    if (str_strcmp(argv[0], "shutdown") == 0 ||
        str_strcmp(argv[0], "poweroff") == 0) {
        dogeio_text_clear_raw();
        dogeio_text_println("Such shutdown, very goodbye.");
        core_shutdown();
        return 0;
    }
    if (str_strcmp(argv[0], "reboot") == 0 ||
        str_strcmp(argv[0], "restart") == 0) {
        dogeio_text_clear_raw();
        dogeio_text_println("Very reboot, much restart.");
        core_reboot();
        return 0;
    }
    if (str_strcmp(argv[0], "dir") == 0 || str_strcmp(argv[0], "ls") == 0) {
        char *directory = NULL;
        int show_hidden = 0;
        for (int i = 1; i < argc; i++) {
            if (str_strcmp(argv[i], "--hidden") == 0 ||
                str_strcmp(argv[i], "--all") == 0 ||
                str_strcmp(argv[i], "-a") == 0 ||
                str_strcmp(argv[i], "-A") == 0) {
                show_hidden = 1;
            } else if (directory == NULL) {
                directory = argv[i];
            } else {
                dogeio_text_println("Usage: dir [location] [--hidden]");
                return 1;
            }
        }
        if (directory == NULL) {
            return fs_list_dir(show_hidden) == 0 ? 0 : 1;
        }
        if (!dogeshell_authorize_path(directory, path, sizeof(path))) {
            return 1;
        }
        return fs_list(path, show_hidden) == 0 ? 0 : 1;
    }
    if (str_strcmp(argv[0], "create") == 0 || str_strcmp(argv[0], "touch") == 0) {
        if (argc != 2) {
            dogeio_text_println("Usage: create <file>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (fs_exists(path)) {
            dogeio_text_println("Error: file exists already.");
            return 1;
        }
        if (fs_create(path) != 0) {
            dogeio_text_println("Error: unable to create file.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "mkdir") == 0) {
        if (argc != 2) {
            dogeio_text_println("Usage: mkdir <directory>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (fs_mkdir(path) != 0) {
            dogeio_text_println("Error: unable to create directory.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "read") == 0 || str_strcmp(argv[0], "cat") == 0) {
        if (argc < 2) {
            dogeio_text_println("Usage: read <file> [file ...]");
            return 1;
        }
        for (int i = 1; i < argc; i++) {
            if (!dogeshell_authorize_path(argv[i], path, sizeof(path))) {
                return 1;
            }
            if (!fs_exists(path) || dogeshell_print_file(path) != 0) {
                dogeio_text_print("Error: unable to read ");
                dogeio_text_println(argv[i]);
                return 1;
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "write") == 0) {
        char text[DOGESHELL_LINE_SIZE];
        if (argc != 2) {
            dogeio_text_println("Usage: write <file>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (!fs_exists(path)) {
            dogeio_text_println("Error: file doesn't exist.");
            return 1;
        }
        dogeio_text_input("> ", text, sizeof(text));
        if (fs_write(path, text) != 0) {
            dogeio_text_println("Error: unable to write file.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "del") == 0 || str_strcmp(argv[0], "rm") == 0) {
        if (argc < 2) {
            dogeio_text_println("Usage: del <file> [file ...]");
            return 1;
        }
        for (int i = 1; i < argc; i++) {
            if (!dogeshell_authorize_path(argv[i], path, sizeof(path))) {
                return 1;
            }
            if (fs_delete(path) != 1) {
                dogeio_text_print("Error: unable to delete ");
                dogeio_text_println(argv[i]);
                return 1;
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "rename") == 0 || str_strcmp(argv[0], "mv") == 0 ||
        str_strcmp(argv[0], "cp") == 0) {
        char source_path[256];
        char destination_path[256];
        int is_copy = str_strcmp(argv[0], "cp") == 0;
        if (argc != 3) {
            dogeio_text_println("Usage: rename <old> <new>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], source_path, sizeof(source_path)) ||
            !dogeshell_authorize_path(argv[2], destination_path,
                                      sizeof(destination_path))) {
            return 1;
        }
        int result;
        if (is_copy) {
            if (str_strcmp(source_path, destination_path) == 0) {
                dogeio_text_println("Error: source and destination are the same.");
                return 1;
            }
            result = fs_copy(source_path, destination_path);
        } else if (str_strcmp(argv[0], "mv") == 0) {
            result = fs_move(source_path, destination_path);
        } else {
            result = fs_rename(source_path, destination_path);
            return result == 1 ? 0 : 1;
        }
        if (result != 0) {
            dogeio_text_println("Error: rename/copy operation failed.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "cd") == 0) {
        char home[128];
        const char *target;
        int print_directory = 0;
        if (argc > 2) {
            dogeio_text_println("Usage: cd [directory]");
            return 1;
        }
        if (argc == 1 || str_strcmp(argv[1], "~") == 0) {
            if (dogeshell_build_home(home, sizeof(home)) != 0) {
                dogeio_text_println("Error: invalid home directory.");
                return 1;
            }
            target = home;
        } else if (str_strcmp(argv[1], "-") == 0) {
            if (dogeshell_old_directory[0] == '\0') {
                dogeio_text_println("Error: previous directory is not set.");
                return 1;
            }
            target = dogeshell_old_directory;
            print_directory = 1;
        } else {
            target = argv[1];
        }
        if (!dogeshell_authorize_path(target, path, sizeof(path))) {
            return 1;
        }
        const char *current_directory = fs_dirname();
        if (current_directory == NULL ||
            dogeshell_copy(dogeshell_old_directory,
                           sizeof(dogeshell_old_directory),
                           current_directory) != 0) {
            dogeio_text_println("Error: unable to determine current directory.");
            return 1;
        }
        if (fs_chdir(path) != 0) {
            dogeio_text_println("Error: directory doesn't exist.");
            return 1;
        }
        if (print_directory) {
            dogeio_text_println(fs_dirname());
        }
        return 0;
    }
    if (str_strcmp(argv[0], "whereami") == 0 || str_strcmp(argv[0], "pwd") == 0) {
        const char *directory = fs_dirname();
        if (directory == NULL) {
            dogeio_text_println("Error: unable to get current directory.");
            return 1;
        }
        dogeio_text_println(directory);
        return 0;
    }
    if (str_strcmp(argv[0], "whoami") == 0) {
        dogeio_text_println(current_user);
        return 0;
    }
    if (str_strcmp(argv[0], "cpuinfo") == 0) {
        dogeio_text_println(cpuid());
        return 0;
    }
    if (str_strcmp(argv[0], "raminfo") == 0) {
        char ram_text[32];
        uint64_to_str(get_ram(), ram_text);
        dogeio_text_print("RAM: ");
        dogeio_text_print(ram_text);
        dogeio_text_println(" bytes");
        return 0;
    }
    if (str_strcmp(argv[0], "fetch") == 0) {
        system_fetch();
        return 0;
    }
    if (str_strcmp(argv[0], "date") == 0) {
        dogeio_text_print(date_get());
        dogeio_text_print(" ");
        dogeio_text_println(time_get());
        return 0;
    }
    if (str_strcmp(argv[0], "time") == 0) {
        dogeio_text_println(time_get());
        return 0;
    }
    if (str_strcmp(argv[0], "edit") == 0) {
        if (argc != 2) {
            dogeio_text_println("Usage: edit <file>");
            return 1;
        }
        if (str_strcmp(argv[1], "--help") == 0) {
            dogeio_text_println("Dogeedit v2.1");
            dogeio_text_println("edit <file>");
            return 0;
        }
        if (str_strcmp(argv[1], "--version") == 0) {
            dogeio_text_println("Dogeedit v2.1");
            return 0;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (!fs_exists(path) && fs_create(path) != 0) {
            dogeio_text_println("Error: unable to create file.");
            return 1;
        }
        system_editor(path);
        return 0;
    }
    if (str_strcmp(argv[0], "settings") == 0) {
        system_settings();
        return 0;
    }
    if (str_strcmp(argv[0], "tab") == 0) {
        sys_switch_terminal();
        return 0;
    }
    if (str_strcmp(argv[0], "pci") == 0) {
        return dogeshell_list_pci();
    }
    if (str_strcmp(argv[0], "calc") == 0) {
        char expression[DOGESHELL_LINE_SIZE];
        char result_text[32];
        if (argc < 2 ||
            dogeshell_join_arguments(argc, argv, 1, expression,
                                     sizeof(expression)) != 0) {
            dogeio_text_println("Usage: calc <expression>");
            return 1;
        }
        str_itoa(util_calc(expression), result_text);
        dogeio_text_println(result_text);
        return 0;
    }
    if (str_strcmp(argv[0], "hexdump") == 0) {
        if (argc != 2) {
            dogeio_text_println("Usage: hexdump <file>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (util_hexdump(path) != 0) {
            dogeio_text_println("Error: file doesn't exist or cannot be read.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "bash") == 0) {
        system_bash();
        return 0;
    }
    if (str_strcmp(argv[0], "run") == 0) {
        if (argc != 2) {
            dogeio_text_println("Usage: run <file>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (!fs_exists(path)) {
            dogeio_text_println("Error: program doesn't exist.");
            return 1;
        }
        system_run_bin(path, MAX_FLAT_BINARY_SIZE);
        return 0;
    }
    if (str_strcmp(argv[0], "uname") == 0) {
        dogeio_text_println("WindogeOS");
        return 0;
    }
    if (str_strcmp(argv[0], "hostname") == 0) {
        dogeio_text_println(computer_name[0] != '\0' ? computer_name : "windoge");
        return 0;
    }

    if (argc == 1) {
        char app_path[160];
        if (dogeshell_find_app(argv[0], app_path, sizeof(app_path)) == 0) {
            system_run_bin(app_path, MAX_FLAT_BINARY_SIZE);
            return 0;
        }
    } else {
        char app_path[160];
        if (dogeshell_find_app(argv[0], app_path, sizeof(app_path)) == 0) {
            dogeio_text_println("Applications do not support command-line arguments yet.");
            return 1;
        }
    }

    dogeio_text_print(argv[0]);
    dogeio_text_println(": command not found :(");
    return 127;
}

int system_dogeshell_ex(char *command) {
    char input[DOGESHELL_LINE_SIZE] = {0};
    char storage[DOGESHELL_LINE_SIZE] = {0};
    char *arguments[DOGESHELL_ARGUMENT_COUNT];
    int argument_count;

    if (command == NULL || command[0] == '\0') {
        return 0;
    }
    if (dogeshell_copy(input, sizeof(input), command) != 0) {
        dogeio_text_println("Error: command is too long.");
        dogeshell_last_status = 2;
        return dogeshell_last_status;
    }
    if (dogeshell_append_history(input) != 0) {
        dogeio_text_println("Warning: unable to save command history.");
    }

    argument_count = dogeshell_tokenize(input, storage, sizeof(storage),
                                        arguments, DOGESHELL_ARGUMENT_COUNT);
    if (argument_count < 0) {
        dogeio_text_println("Syntax error: unmatched quote, escape, or too many arguments.");
        dogeshell_last_status = 2;
        return dogeshell_last_status;
    }
    dogeshell_last_status = dogeshell_execute(argument_count, arguments);
    return dogeshell_last_status;
}

void system_dogeshell(void) {
    char input[DOGESHELL_LINE_SIZE];
    char home_path[128];

    dogeshell_exit_requested = 0;
    dogeshell_last_status = 0;
    dogeshell_old_directory[0] = '\0';
    if (dogeshell_build_home(home_path, sizeof(home_path)) != 0 ||
        fs_chdir(home_path) != 0) {
        dogeio_text_println("Dogeshell: unable to enter the user home directory.");
    }

    while (!dogeshell_exit_requested) {
        const char *current_directory = fs_dirname();
        dogeio_text_color_change(0xFF00FF00);
        dogeio_text_print(current_user);
        dogeio_text_color_change(saved_color);
        dogeio_text_print(" (");
        if (current_directory == NULL) {
            dogeio_text_print("/");
        } else if (str_strcmp(current_directory, home_path) == 0) {
            dogeio_text_print("~");
        } else {
            dogeio_text_print(current_directory);
        }
        dogeio_text_print(") ");

        if (dogeshell_last_status != 0) {
            char status_text[16];
            str_itoa(dogeshell_last_status, status_text);
            dogeio_text_color_change(0xFFFF0000);
            dogeio_text_print("[");
            dogeio_text_print(status_text);
            dogeio_text_print("] ");
            dogeio_text_color_change(saved_color);
        }

        dogeio_text_input("> ", input, sizeof(input));
        if (input[0] != '\0') {
            system_dogeshell_ex(input);
        }
    }
}
