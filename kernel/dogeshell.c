#include <boot/kernel.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <system.h>
#include <core.h>
#include <basicutil.h>
#include <boot/limine.h>
#include <bool.h>

extern volatile struct limine_memmap_request memmap_request;
extern char current_user[64];
extern char settings_path[];

char dogeshell_app_names[64][256];
uint32_t saved_color = 0xFFCCCCCC;

void get_all_fucking_ram(void) {
    if (memmap_request.response == NULL || memmap_request.response->entry_count == 0) {
        return; 
    }

    uint64_t entries = memmap_request.response->entry_count;
    uint64_t type_counts[8] = {0}; 

    for (uint64_t i = 0; i < entries; i++) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (entry->type < 8) {
            type_counts[entry->type] += entry->length;
        }
    }

    char debug_buf[32];
    dogeio_println("\n--- Memory Map Breakdown ---");
    for (int t = 0; t < 8; t++) {
        dogeio_print("Type ");
        str_kutoa((uint64_t)t, debug_buf); 
        dogeio_print(debug_buf);
        dogeio_print(": ");
        str_kutoa(type_counts[t] / 1024, debug_buf); 
        dogeio_print(debug_buf);
        dogeio_println(" KB");
    }
}

int dogeshell_list_pci(void) {
    dogeio_println("ADDRESS    IDENTITY DESCRIPTION & [VENDOR:DEVICE ID]");
    dogeio_println("---------------------------------------------------------");
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
                dogeio_print(":");
                dogeio_print_hex8(slot);
                dogeio_print(".");
                dogeio_print(function_text);
                dogeio_print("    ");
                dogeio_print(pci_class_to_name(class_code));
                dogeio_print(" [");
                dogeio_print_hex16(vendor);
                dogeio_print(":");
                dogeio_print_hex16(device);
                dogeio_println("]");
            }
        }
    }
    dogeio_println("\nPress any key to return...");
    return 0;
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
        dogeio_println("Error: invalid or overlong path.");
        return 0;
    }
    if (!system_can_access_path(current_user, resolved)) {
        dogeio_println("Error: permission denied.");
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
        "===============================================================================================================================================================",
        " BASIC FUNCTIONS                                                | FILE SYSTEM                                                                                  ",
        "==================================================================+============================================================================================",
        " print/echo <text>        | print text                          | dir [path] [--hidden]      | list a directory                                               ",
        " clear                    | clear the terminal                  | read <file>                | print file contents                                            ",
        " ver                      | show shell version                  | write <file>               | replace a file with one line of text                           ",
        " help                     | show this help                      | create <file>              | create an empty file                                           ",
        " history                  | show command history                | mkdir <directory>          | create a directory                                             ",
        " clear-history            | clear command history               | del <file>                 | delete a file                                                  ",
        " time, date               | show current time or date           | rename <old> <new>         | rename a file                                                  ",
        " shutdown, reboot         | power off or restart the system     | whereami                   | print the current directory                                    ",
        "                                                                | cd [directory]             | change directory                                               ",
        "==================================================================+============================================================================================",
        " SYSTEM AND UTILITIES                                           |                                                                                              ",
        "==================================================================+============================================================================================",
        " whoami, cpuinfo, raminfo | user and system info                | apps                       | list installed apps in /apps                                   ",
        " fetch, settings, tab     | system UI and terminal controls     | calc <expression>          | evaluate a calculator expression                               ",
        " edit <file>              | open the text editor                | hexdump <file>             | display file bytes                                             ",
        " pci                      | list PCI devices                    | run <file>                 | run a flat binary                                              ",
        " bash                     | open Bash; exit returns here        | <app>[.bin] [args...]      | run an app from /apps                                          ",
        " exit                     | leave Dogeshell                     | dogestore                  | install and uninstall apps from dogestore                      ",
        "===============================================================================================================================================================",
        " Use ; between commands. Quoted or escaped semicolons are literal. Paths may be quoted; use ~ for your home directory."
    };
    for (size_t i = 0; i < sizeof(help) / sizeof(help[0]); i++) {
        dogeio_println(help[i]);
    }
}

static int dogeshell_show_history(void) {
    static char contents[8192];
    char path[160];
    int bytes_read;

    if (dogeshell_history_path(path, sizeof(path)) != 0) {
        dogeio_println("History error: path is too long.");
        return 1;
    }
    bytes_read = fs_read(path, contents, sizeof(contents) - 1);
    if (bytes_read < 0) {
        dogeio_println("No history available.");
        return 1;
    }
    if (bytes_read == 0) {
        dogeio_println("No history available.");
        return 0;
    }
    if (bytes_read >= (int)sizeof(contents)) {
        dogeio_println("History error: invalid file size.");
        return 1;
    }
    contents[bytes_read] = '\0';
    dogeio_print(contents);
    if (contents[bytes_read - 1] != '\n') {
        dogeio_println("");
    }
    return 0;
}

static int dogeshell_print_file(const char *path) {
    static char contents[8192];
    int bytes_read = fs_read((char *)path, contents, sizeof(contents) - 1);
    if (bytes_read < 0 || bytes_read >= (int)sizeof(contents)) {
        dogeio_println("Unable to read file.");
        return 1;
    }
    contents[bytes_read] = '\0';
    dogeio_print(contents);
    if (bytes_read == 0 || contents[bytes_read - 1] != '\n') {
        dogeio_println("");
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
        dogeshell_append(path, sizeof(path), &path_length, filename) != 0) {
        return 1;
    }
    if (!fs_exists(path)) {
        path[0] = '/';
        path[1] = '\0';
        path_length = 1;
        if (dogeshell_append(path, sizeof(path), &path_length, filename) != 0 ||
            !fs_exists(path)) {
            return 1;
        }
    }
    if (dogeshell_copy(app_path, app_path_capacity, path) != 0) {
        return 1;
    }
    return 0;
}

static int dogeshell_list_apps(void) {
    int app_count = fs_list_files(
        "/apps", dogeshell_app_names,
        sizeof(dogeshell_app_names) / sizeof(dogeshell_app_names[0]));
    if (app_count < 0) {
        dogeio_println("Error: unable to list /apps.");
        return 1;
    }

    int printed_count = 0;
    for (int i = 0; i < app_count; i++) {
        size_t name_length = str_strlen(dogeshell_app_names[i]);
        if (name_length < 4) {
            continue;
        }

        const char *extension = dogeshell_app_names[i] + name_length - 4;
        if (extension[0] != '.' ||
            (extension[1] != 'b' && extension[1] != 'B') ||
            (extension[2] != 'i' && extension[2] != 'I') ||
            (extension[3] != 'n' && extension[3] != 'N')) {
            continue;
        }

        dogeio_print("  ");
        dogeio_println(dogeshell_app_names[i]);
        printed_count++;
    }

    if (printed_count == 0) {
        dogeio_println("No apps are installed in /apps.");
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
        if (argc == 1) {
            dogeshell_print_help();
        } else {
            if (str_strncmp(argv[1], "dogescript", 10) == 0) {
                system_dogeshell_ex("pages /apps/text/help/dogescript_help");
            } else if (str_strncmp(argv[1], "general", 7) == 0) {
                system_dogeshell_ex("pages /apps/text/help/general_help");
            } else if (str_strncmp(argv[1], "dogeedit", 8) == 0) {
                system_dogeshell_ex("pages /apps/text/help/dogeedit_help");
            } else if (str_strncmp(argv[1], "bash", 4) == 0) {
                system_dogeshell_ex("pages /apps/text/help/bash_help");
            } else if (str_strncmp(argv[1], "syscalls", 8) == 0) {
                system_dogeshell_ex("pages /apps/text/help/syscalls");
            }
            
            else {
                dogeio_println("Help for that doesn't exists :(");
                return 1;
            }
        }
        
        return 0;
    }

    if (str_strcmp(argv[0], "print") == 0 || str_strcmp(argv[0], "echo") == 0) {
        char output[DOGESHELL_LINE_SIZE];
        if (dogeshell_join_arguments(argc, argv, 1, output, sizeof(output)) != 0) {
            dogeio_println("Error: text is too long.");
            return 1;
        }
        dogeio_println(output);
        return 0;
    }
    if (str_strcmp(argv[0], "clear") == 0) {
        dogeio_clear();
        return 0;
    }
    if (str_strcmp(argv[0], "ver") == 0) {
        dogeio_println(dogeshell_version);
        return 0;
    }
    if (str_strcmp(argv[0], "history") == 0) {
        return dogeshell_show_history();
    }
    if (str_strcmp(argv[0], "clear-history") == 0) {
        char history_path[160];
        if (dogeshell_history_path(history_path, sizeof(history_path)) != 0) {
            dogeio_println("History error: path is too long.");
            return 1;
        }
        if (fs_exists(history_path) && fs_delete(history_path) != 1) {
            dogeio_println("History error: unable to delete history.");
            return 1;
        }
        if (fs_create(history_path) != 0) {
            dogeio_println("History error: unable to create history.");
            return 1;
        }
        dogeio_println("History cleared.");
        return 0;
    }
    if (str_strcmp(argv[0], "shutdown") == 0 ||
        str_strcmp(argv[0], "poweroff") == 0) {
        dogeio_clear_raw();
        dogeio_println("Such shutdown, very goodbye.");
        core_shutdown();
        return 0;
    }
    if (str_strcmp(argv[0], "reboot") == 0 ||
        str_strcmp(argv[0], "restart") == 0) {
        dogeio_clear_raw();
        dogeio_println("Very reboot, much restart.");
        core_reboot();
        return 0;
    }

    if (str_strcmp(argv[0], "dogestore") == 0) {
        if (argc < 2) {
            dogeio_println("Usage: dogestore <command> [args...]");
            return 1;
        }

        return kernel_package(argc + 1, argv + 1);
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
                dogeio_println("Usage: dir [location] [--hidden]");
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

    if (str_strcmp(argv[0], "pack") == 0) {
        if (argc == 2) {
            if (str_strcmp(argv[1], "--help") == 0) {
                dogeio_println("RLEPACK (Kernel Package Utility)");
                dogeio_println("Usage: pack [file]");
                return 1;
            }
        }

        if (argc != 2 || argv[1] == NULL || argv[1][0] == '\0') {
            dogeio_println("Usage: pack [file]");
            return 2;
        }

        char *target_filename = argv[1];
        uint64_t file_size = 0;
        bool is_dir = false;

        if (fs_get_info(target_filename, &file_size, &is_dir) != 0) {
            dogeio_println("Error: Target file does not exist.");
            return -1;
        }

        if (is_dir) {
            dogeio_println("Error: Cannot pack a directory.");
            return -1;
        }

        uint64_t len = str_strlen(target_filename);
        char out_name[256];
        for (int i = 0; i < 256; i++) out_name[i] = '\0';

        if (len >= 250) {
            dogeio_println("Error: Filename too long.");
            return -1;
        }

        if (len > 4 && str_strcmp(target_filename + len - 4, ".rle") == 0) {
            dogeio_println("Action: Decompressing archive stream...");
            
            str_strcpy(out_name, target_filename);
            out_name[len - 4] = '\0';

            kernel_rle_decompress(target_filename, file_size, out_name);
        } 
        else {
            dogeio_println("Action: Compressing raw binary target...");

            str_strcpy(out_name, target_filename);
            str_strcat(out_name, ".rle");

            kernel_rle_compress(target_filename, file_size, out_name);
        }
        return 0;
    }

    if (str_strcmp(argv[0], "create") == 0 || str_strcmp(argv[0], "touch") == 0) {
        if (argc != 2) {
            dogeio_println("Usage: create <file>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (fs_exists(path)) {
            dogeio_println("Error: file exists already.");
            return 1;
        }
        if (fs_create(path) != 0) {
            dogeio_println("Error: unable to create file.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "mkdir") == 0) {
        if (argc != 2) {
            dogeio_println("Usage: mkdir <directory>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (fs_mkdir(path) != 0) {
            dogeio_println("Error: unable to create directory.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "read") == 0 || str_strcmp(argv[0], "cat") == 0) {
        if (argc < 2) {
            dogeio_println("Usage: read <file> [file ...]");
            return 1;
        }
        for (int i = 1; i < argc; i++) {
            if (!dogeshell_authorize_path(argv[i], path, sizeof(path))) {
                return 1;
            }
            if (!fs_exists(path) || dogeshell_print_file(path) != 0) {
                dogeio_print("Error: unable to read ");
                dogeio_println(argv[i]);
                return 1;
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "write") == 0) {
        char text[DOGESHELL_LINE_SIZE];
        if (argc != 2) {
            dogeio_println("Usage: write <file>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (!fs_exists(path)) {
            dogeio_println("Error: file doesn't exist.");
            return 1;
        }
        dogeio_input("> ", text, sizeof(text));
        if (fs_write(path, text) != 0) {
            dogeio_println("Error: unable to write file.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "del") == 0 || str_strcmp(argv[0], "rm") == 0) {
        if (argc < 2) {
            dogeio_println("del: missing file operand");
            return 1;
        }
        for (int i = 1; i < argc; i++) {
            if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
                return 1;
            }

            if (str_strcmp(argv[i], "/") == 0 || str_strcmp(argv[i], "/.") == 0) {
                dogeio_println("[Error] no deleting the entire drive.");
                return 1;
            }

            if (str_strcmp(argv[i], "/.windoge") == 0 || 
                str_strcmp(argv[i], ".windoge") == 0 || 
                str_strcmp(argv[i], "/./.windoge") == 0 ||
                str_strcmp(argv[i], "./.windoge") == 0) {
                dogeio_println("[Error] no deleting boot file");
                return 1;
            }

            if (str_strcmp(argv[i], "/system") == 0 || (str_strcmp(argv[i], "system") == 0 || str_strcmp(fs_dirname(), "/") == 0) || 
                str_strcmp(argv[i], "/./system") == 0 || str_strcmp(argv[i], "./system") == 0) {
                dogeio_println("[Error] no deleting system files");
                return 1;
            }

            if (str_strcmp(argv[i], "/apps") == 0 || (str_strcmp(argv[i], "apps") == 0 && str_strcmp(fs_dirname(), "/") == 0) 
            || str_strcmp(argv[i], "/./apps") == 0 || str_strcmp(argv[i], "./apps") == 0) {
                dogeio_println("[Error] no deleting system files");
                return 1;
            }
            
            if (str_strcmp(argv[i], "/users") == 0 || (str_strcmp(argv[i], "users") == 0 && str_strcmp(fs_dirname(), "/") == 0) 
            || str_strcmp(argv[i], "/./users") == 0 || str_strcmp(argv[i], "./users") == 0) {
                dogeio_println("[Error] no deleting system files");
                return 1;
            }

            if (fs_delete(argv[i]) != 1) {
                dogeio_print("bash: rm: cannot remove ");
                dogeio_println(argv[i]);
                return 1;
            }
        }
        return 0 ;
    }
    if (str_strcmp(argv[0], "rename") == 0 || str_strcmp(argv[0], "mv") == 0 ||
        str_strcmp(argv[0], "cp") == 0) {
        char source_path[256];
        char destination_path[256];
        int is_copy = str_strcmp(argv[0], "cp") == 0;
        if (argc != 3) {
            dogeio_println("Usage: rename <old> <new>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], source_path, sizeof(source_path)) ||
            !dogeshell_authorize_path(argv[2], destination_path,
                                      sizeof(destination_path))) {
            return 1;
        }

        if (str_strcmp(argv[1], "/system") == 0 || (str_strcmp(argv[1], "system") == 0 || str_strcmp(fs_dirname(), "/") == 0) || 
            str_strcmp(argv[1], "/./system") == 0 || str_strcmp(argv[1], "./system") == 0) {
            dogeio_println("[Error] no deleting system files");
            return 1;
        }

        if (str_strcmp(argv[1], "/apps") == 0 || (str_strcmp(argv[1], "apps") == 0 && str_strcmp(fs_dirname(), "/") == 0) 
        || str_strcmp(argv[1], "/./apps") == 0 || str_strcmp(argv[1], "./apps") == 0) {
            dogeio_println("[Error] no deleting system files");
            return 1;
        }
            
        if (str_strcmp(argv[1], "/users") == 0 || (str_strcmp(argv[1], "users") == 0 && str_strcmp(fs_dirname(), "/") == 0) 
        || str_strcmp(argv[1], "/./users") == 0 || str_strcmp(argv[1], "./users") == 0) {
            dogeio_println("[Error] no deleting system files");
            return 1;
        }

        int result;
        if (is_copy) {
            if (str_strcmp(source_path, destination_path) == 0) {
                dogeio_println("Error: source and destination are the same.");
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
            dogeio_println("Error: rename/copy operation failed.");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "cd") == 0) {
        char home[128];
        const char *target;
        int print_directory = 0;
        if (argc > 2) {
            dogeio_println("Usage: cd [directory]");
            return 1;
        }
        if (argc == 1 || str_strcmp(argv[1], "~") == 0) {
            if (dogeshell_build_home(home, sizeof(home)) != 0) {
                dogeio_println("Error: invalid home directory.");
                return 1;
            }
            target = home;
        } else if (str_strcmp(argv[1], "-") == 0) {
            if (dogeshell_old_directory[0] == '\0') {
                dogeio_println("Error: previous directory is not set.");
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
            dogeio_println("Error: unable to determine current directory.");
            return 1;
        }
        if (fs_chdir(path) != 0) {
            dogeio_println("Error: directory doesn't exist.");
            return 1;
        }
        if (print_directory) {
            dogeio_println(fs_dirname());
        }
        return 0;
    }
    if (str_strcmp(argv[0], "whereami") == 0 || str_strcmp(argv[0], "pwd") == 0) {
        const char *directory = fs_dirname();
        if (directory == NULL) {
            dogeio_println("Error: unable to get current directory.");
            return 1;
        }
        dogeio_println(directory);
        return 0;
    }
    if (str_strcmp(argv[0], "whoami") == 0) {
        dogeio_println(current_user);
        return 0;
    }
    if (str_strcmp(argv[0], "cpuinfo") == 0) {
        dogeio_println(cpuid());
        return 0;
    }
    if (str_strcmp(argv[0], "raminfo") == 0) {
        get_all_fucking_ram();
        return 0;
    }
    if (str_strcmp(argv[0], "fetch") == 0) {
        system_fetch();
        return 0;
    }
    if (str_strcmp(argv[0], "date") == 0) {
        dogeio_print(date_get());
        dogeio_print(" ");
        dogeio_println(time_get());
        return 0;
    }
    if (str_strcmp(argv[0], "time") == 0) {
        dogeio_println(time_get());
        return 0;
    }
    if (str_strcmp(argv[0], "edit") == 0) {
        if (argc != 2) {
            dogeio_println("Usage: edit <file>");
            return 1;
        }
        if (str_strcmp(argv[1], "--help") == 0) {
            dogeio_println("Dogeedit v2.2");
            dogeio_println("edit <file>");
            return 0;
        }
        if (str_strcmp(argv[1], "--version") == 0) {
            dogeio_println("Dogeedit v2.3");
            return 0;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (!fs_exists(path) && fs_create(path) != 0) {
            dogeio_println("Error: unable to create file.");
            return 1;
        }
        char editor_path[160];
        if (dogeshell_find_app("editor", editor_path,
                               sizeof(editor_path)) != 0) {
            dogeio_println("Error: editor app is not installed.");
            return 127;
        }
        char *editor_args[] = {path};
        system_run_bin_args(editor_path, MAX_FLAT_BINARY_SIZE, 1,
                            editor_args);
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
    if (str_strcmp(argv[0], "apps") == 0) {
        if (argc != 1) {
            dogeio_println("Usage: apps");
            return 1;
        }
        return dogeshell_list_apps();
    }
    
    if (str_strcmp(argv[0], "bash") == 0) {
        system_bash();
        return 0;
    }
    if (str_strcmp(argv[0], "run") == 0) {
        if (argc != 2) {
            dogeio_println("Usage: run <file>");
            return 1;
        }
        if (!dogeshell_authorize_path(argv[1], path, sizeof(path))) {
            return 1;
        }
        if (!fs_exists(path)) {
            dogeio_println("Error: program doesn't exist.");
            return 1;
        }
        system_exec(path);
        return 0;
    }
    if (str_strcmp(argv[0], "uname") == 0) {
        dogeio_println("WindogeOS");
        return 0;
    }
    if (str_strcmp(argv[0], "hostname") == 0) {
        dogeio_println(computer_name[0] != '\0' ? computer_name : "windoge");
        return 0;
    }
    if (str_strcmp(argv[0], "sleep") == 0) {
        fs_set_auth_override(1);
        system_run_bin("/system/sleep.bin", 512);
        fs_set_auth_override(0);
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
            system_run_bin_args(app_path, MAX_FLAT_BINARY_SIZE, argc - 1,
                                argv + 1);
            return 0;
        }
    }

    dogeio_print(argv[0]);
    dogeio_println(": command not found :(");
    return 127;
}

int system_dogeshell_ex(char *command) {
    char input[DOGESHELL_LINE_SIZE] = {0};
    size_t separators[DOGESHELL_LINE_SIZE];
    size_t separator_count = 0;
    size_t input_length;
    size_t segment_start = 0;
    char quote = '\0';
    int status = 0;

    if (command == NULL || command[0] == '\0') {
        return 0;
    }
    if (dogeshell_copy(input, sizeof(input), command) != 0) {
        dogeio_println("Error: command is too long.");
        dogeshell_last_status = 2;
        return dogeshell_last_status;
    }
    if (dogeshell_append_history(input) != 0) {
        dogeio_println("Warning: unable to save command history.");
    }

    input_length = dogeshell_strlen(input);
    for (size_t i = 0; i < input_length; i++) {
        char character = input[i];
        if (character == '\\' && quote != '\'') {
            if (i + 1 == input_length) {
                dogeio_println("Syntax error: trailing escape.");
                dogeshell_last_status = 2;
                return dogeshell_last_status;
            }
            i++;
            continue;
        }
        if (quote != '\0') {
            if (character == quote) {
                quote = '\0';
            }
            continue;
        }
        if (character == '\'' || character == '"') {
            quote = character;
        } else if (character == ';') {
            separators[separator_count++] = i;
        }
    }
    if (quote != '\0') {
        dogeio_println("Syntax error: unmatched quote.");
        dogeshell_last_status = 2;
        return dogeshell_last_status;
    }

    for (size_t segment_index = 0;
         segment_index <= separator_count && !dogeshell_exit_requested;
         segment_index++) {
        char storage[DOGESHELL_LINE_SIZE] = {0};
        char *arguments[DOGESHELL_ARGUMENT_COUNT];
        size_t segment_end = segment_index < separator_count
                                 ? separators[segment_index]
                                 : input_length;
        int argument_count;

        input[segment_end] = '\0';
        argument_count = dogeshell_tokenize(input + segment_start, storage,
                                            sizeof(storage), arguments,
                                            DOGESHELL_ARGUMENT_COUNT);
        if (argument_count < 0) {
            dogeio_println("Syntax error: too many arguments.");
            dogeshell_last_status = 2;
            return dogeshell_last_status;
        }
        if (argument_count > 0) {
            status = dogeshell_execute(argument_count, arguments);
            dogeshell_last_status = status;
        }
        segment_start = segment_end + (segment_index < separator_count ? 1U : 0U);
    }

    dogeshell_last_status = status;
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
        dogeio_println("Dogeshell: unable to enter the user home directory.");
    }

    while (!dogeshell_exit_requested) {
        const char *current_directory = fs_dirname();
        dogeio_color_change(0xFF00FF00);
        dogeio_print(current_user);
        dogeio_color_change(saved_color);
        dogeio_print(" (");
        if (current_directory == NULL) {
            dogeio_print("/");
        } else if (str_strcmp(current_directory, home_path) == 0) {
            dogeio_print("~");
        } else {
            dogeio_print(current_directory);
        }
        dogeio_print(") ");

        if (dogeshell_last_status != 0) {
            char status_text[16];
            str_itoa(dogeshell_last_status, status_text);
            dogeio_color_change(0xFFFF0000);
            dogeio_print("[");
            dogeio_print(status_text);
            dogeio_print("] ");
            dogeio_color_change(saved_color);
        }

        dogeio_input("> ", input, sizeof(input));
        if (input[0] != '\0') {
            system_dogeshell_ex(input);
        }
        input[0] = '\0';
    }
}
