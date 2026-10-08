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

char user[64];
static char buffer[8192];

char* help[] = {
    "Basic Functions",
    "=======================================================",
    "print  [text]           | prints text",
    "clear                   | clears terminal",
    "ver                     | shows shell version",
    "history                 | shows shell history",
    "clear-history           | clears shell history",
    "time                    | shows time",
    "shutdown                | shuts down system",
    "reboot                  | restarts/reboot the system",
    "=======================================================",
    "",
    "File System",
    "=======================================================",
    "dir    [location]       | lists folder (--hidden for all)",
    "read   [file]           | outputs file contents",
    "write  [file]           | writes contents into files",
    "del    [file]           | deletes a file",
    "cd     [location]       | change folder location",
    "create [file]           | creates a new file",
    "mkdir  [foldername]     | create a folder",
    "rename [file] [name]    | renames a file",
    "whereami                | shows current location",
    "=======================================================",
    "",
    "System Information",
    "=======================================================",
    "whoami                  | shows current user",
    "cpuinfo                 | show CPU name",
    "raminfo                 | show RAM amount in bytes",
    "date                    | shows the date and time",
    "fetch                   | just like fastfetch",
    "=======================================================",
    "",
    "System Utilities",
    "=======================================================",
    "settings                | change system preferences",
    "edit   [file]           | edits a file",
    "pci                     | lists all pci devices",
    "tab                     | switch terminal tabs",
    "hexdump                 | you know this... right?",
    "calc                    | calculator, just calculator.",
    "bash                    | runs bash shell",
    "run    [file]           | run a program",
    "name[.bin]              | run an app from /apps (no arguments yet)",
    "=======================================================",
};

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
    system_dogeshell_ex("run syscall_test.bin");
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

static void get_history_path(char* dest) {
    str_strcpy(dest, "/users/");
    str_strcat(dest, current_user);
    str_strcat(dest, "/.history");
}

static const char* get_cmd_arg(const char* command, const char* cmd_name) {
    size_t len = str_strlen(cmd_name);
    if (str_strcmp(command, cmd_name) == 0) {
        return "";
    }
    if (str_startswith(command, cmd_name) && command[len] == ' ') {
        const char* arg = command + len + 1;
        while (*arg == ' ') arg++;
        return arg;
    }
    return NULL;
}

static int run_app_command(const char *command) {
    char app_name[128];
    size_t name_length = 0;

    while (command[name_length] != '\0' && command[name_length] != ' ' &&
           command[name_length] != '\t') {
        if (name_length >= sizeof(app_name) - 1 ||
            command[name_length] == '/' || command[name_length] == '\\') {
            return 1;
        }
        app_name[name_length] = command[name_length];
        name_length++;
    }
    app_name[name_length] = '\0';
    if (name_length == 0) {
        return 1;
    }

    const char *arguments = command + name_length;
    while (*arguments == ' ' || *arguments == '\t') {
        arguments++;
    }
    if (*arguments != '\0') {
        dogeio_text_println("Applications do not support command-line arguments yet.");
        return -1;
    }

    bool has_bin_extension = false;
    if (name_length >= 4) {
        const char *extension = app_name + name_length - 4;
        has_bin_extension =
            extension[0] == '.' &&
            (extension[1] == 'b' || extension[1] == 'B') &&
            (extension[2] == 'i' || extension[2] == 'I') &&
            (extension[3] == 'n' || extension[3] == 'N');
    }
    if (!has_bin_extension) {
        if (name_length + 4 >= sizeof(app_name)) {
            return 1;
        }
        app_name[name_length++] = '.';
        app_name[name_length++] = 'b';
        app_name[name_length++] = 'i';
        app_name[name_length++] = 'n';
        app_name[name_length] = '\0';
    }

    char path[sizeof("/apps/") + sizeof(app_name)];
    str_strcpy(path, "/apps/");
    str_strcat(path, app_name);
    if (!fs_exists(path)) {
        return 1;
    }

    system_run_bin(path, MAX_FLAT_BINARY_SIZE);
    return 0;
}

int system_dogeshell_ex(char* command) {
    int handled = 1;
    const char* arg = NULL;

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    if ((arg = get_cmd_arg(command, "print")) != NULL) {
        dogeio_text_println((char*)arg);
        handled = 0;
    }
    else if (str_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 0;
    }
    else if (str_strcmp(command, "ver") == 0) {
        dogeio_text_println(dogeshell_version);
        handled = 0;
    }
    else if (str_strcmp(command, "history") == 0) {
        char hist_path[128];
        get_history_path(hist_path);
        
        int bytes_read = fs_read(hist_path, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            dogeio_text_println("No history available.");
        } else {
            buffer[bytes_read] = '\0';
            char *line = buffer;
            for (int i = 0; i < bytes_read; i++) {
                if (buffer[i] == '\n') {
                    buffer[i] = '\0';
                    if (str_strlen(line) > 0) {
                        dogeio_text_println(line);
                    }
                    line = buffer + i + 1;
                }
            }
            if (*line != '\0') {
                dogeio_text_println(line);
            }
        }
        handled = 0;
    }
    else if (str_strcmp(command, "clear-history") == 0) {
        char hist_path[128];
        get_history_path(hist_path);
        fs_delete(hist_path);
        fs_create(hist_path);
        dogeio_text_println("History cleared.");
        handled = 0;
    }
    else if (str_strcmp(command, "help") == 0) {
        size_t count = sizeof(help) / sizeof(help[0]);
        for (size_t i = 0; i < count; i++) {
            dogeio_text_println(help[i]);
        }
        handled = 0;
    }
    else if (str_strcmp(command, "shutdown") == 0 || str_strcmp(command, "poweroff") == 0) {
        core_shutdown();
        handled = 0;
    }
    else if (str_strcmp(command, "reboot") == 0 || str_strcmp(command, "restart") == 0) {
        core_reboot();
        handled = 0;
    }
    
    else if (str_startswith(command, "dir")) {
        char* args = command + 3;
        while (*args == ' ') args++;

        char clean_args[128];
        str_strncpy(clean_args, args, sizeof(clean_args) - 1);
        clean_args[sizeof(clean_args) - 1] = '\0';

        int len = (int)str_strlen(clean_args);
        while (len > 0 && (clean_args[len - 1] == '\n' || clean_args[len - 1] == '\r' || clean_args[len - 1] == ' ')) {
            clean_args[--len] = '\0';
        }

        int show_hidden = 0;
        char directory[128] = {0};

        if (clean_args[0] == '\0') {
            fs_list_dir(0);
        } else if (str_strcmp(clean_args, "--hidden") == 0) {
            fs_list_dir(1);
        } else {
            if (str_startswith(clean_args, "--hidden ")) {
                show_hidden = 1;
                str_strcpy(directory, clean_args + 9);
            } else {
                int clen = (int)str_strlen(clean_args);
                if (clen > 8 && str_strcmp(clean_args + clen - 8, "--hidden") == 0 && clean_args[clen - 9] == ' ') {
                    show_hidden = 1;
                    str_strncpy(directory, clean_args, (size_t)clen - 9);
                    directory[clen - 9] = '\0';
                } else {
                    str_strcpy(directory, clean_args);
                }
            }

            if (directory[0] != '\0') {
                fs_list(directory, show_hidden);
            } else {
                fs_list_dir(show_hidden);
            }
        }
        handled = 0;
    }

    else if ((arg = get_cmd_arg(command, "create")) != NULL) {
        if (str_strlen(arg) == 0) {
            dogeio_text_println("Error: no filename specified.");
            handled = -1;
        } else if (fs_exists((char*)arg)) {
            dogeio_text_println("Error: file exists already.");
            handled = -1;
        } else {
            if (fs_create((char*)arg) == -1) {
                dogeio_text_println("Much Sad: unable to create file.");
                handled = -1;
            } else {
                handled = 0;
            }
        }
    }
    else if ((arg = get_cmd_arg(command, "mkdir")) != NULL) {
        if (str_strlen(arg) == 0) {
            dogeio_text_println("Error: no folder name specified.");
            handled = -1;
        } else if (fs_exists((char*)arg)) {
            dogeio_text_println("Error: folder exists already.");
            handled = -1;
        } else {
            fs_mkdir((char*)arg);
            handled = 0;
        }
    }
    else if ((arg = get_cmd_arg(command, "read")) != NULL) {
        if (str_strlen(arg) == 0) {
            dogeio_text_println("Error: no file specified.");
            handled = -1;
        } else if (!fs_exists((char*)arg)) {
            dogeio_text_println("Much Sad: file doesn't exist.");
            handled = -1;
        } else {
            int bytes_read = fs_read((char*)arg, buffer, sizeof(buffer) - 1);
            if (bytes_read < 0) {
                dogeio_text_println("Not Wow: unable to read file.");
                handled = -1;
            } else {
                buffer[bytes_read] = '\0';
                char *line = buffer;
                size_t processed = 0;
                while ((int)processed < bytes_read) {
                    dogeio_text_println(line);
                    size_t line_len = str_strlen(line);
                    processed += line_len + 1;
                    line += line_len + 1;
                }
                handled = 0;
            }
        }
    }
    
    else if (str_startswith(command, "cd")) {
        char* target = command + 3;
        if (str_startswith(target, "/system") || (str_startswith(target, "system") && str_strcmp(fs_dirname(), "/") == 0)) {
            dogeio_text_println("Error: permission denied, because it's a system folder :(");
            handled = -1;
        } else if (str_strcmp(target, "system") == 0 && str_strcmp(fs_dirname(), "/") == 0) {
			dogeio_text_println("Error: permission denied, because it's a system folder :(");
			handled = -1;
        } else if (str_strcmp(fs_dirname(), "/users") == 0 && str_strcmp(current_user, target) != 0) {
            if (str_strcmp(target, "/") == 0 || str_strcmp(target, "..") == 0) {
                if (!fs_chdir(target)) {
                    handled = 0;
                } else {
                    dogeio_text_println("Error: much folder doesn't exist :(");
                    handled = -2;
                }
            } else {
                dogeio_text_println("Error: permission denied, because why are you trying to see other accounts?");
                handled = -1;
            }
        } else {
            if (!fs_chdir(target)) {
                handled = 0;
            } else {
                dogeio_text_println("Error: much folder doesn't exist :(");
                handled = -2;
            }
        }
    }   
    
    else if ((arg = get_cmd_arg(command, "write")) != NULL) {
        if (str_strlen(arg) == 0) {
            dogeio_text_println("Much Error: no file specified.");
            handled = -1;
        } else if (!fs_exists((char*)arg)) {
            dogeio_text_println("Much Error: file doesn't exist.");
            handled = -1;
        } else {
            dogeio_text_input("> ", buffer, sizeof(buffer));
            fs_write((char*)arg, buffer);
            handled = 0;
        }
    }

    else if ((arg = get_cmd_arg(command, "rename")) != NULL) {
        char first_arg[64] = {0};
        char second_arg[64] = {0};
        
        int i = 0;
        while (arg[i] != '\0' && arg[i] != ' ' && i < 63) {
            first_arg[i] = arg[i];
            i++;
        }
        first_arg[i] = '\0';

        while (arg[i] == ' ') i++;

        str_strncpy(second_arg, arg + i, 63);
        second_arg[63] = '\0';

        if (str_strlen(first_arg) == 0 || str_strlen(second_arg) == 0) {
            dogeio_text_println("Error: usage: rename [old_file] [new_name]");
            handled = -1;
        } else if (fs_exists(first_arg)) {
            fs_rename(first_arg, second_arg);
            handled = 0;
        } else {
            dogeio_text_println("Error: file doesn't exist.");
            handled = -1;
        }
    }
    else if ((arg = get_cmd_arg(command, "del")) != NULL) {
        if (str_strlen(arg) == 0) {
            dogeio_text_println("Error: no file specified.");
            handled = -1;
        } else if (!fs_exists((char*)arg)) {
            dogeio_text_println("Error: file doesn't exist, could be a typo.");
            handled = -1;
        } else {
            fs_delete((char*)arg);
            handled = 0;
        }
    }
    else if (str_strcmp(command, "whereami") == 0) {
        dogeio_text_println(fs_dirname());
        handled = 0;
    }
    else if (str_strcmp(command, "whoami") == 0) {
        dogeio_text_println(current_user);
        handled = 0;
    }
    else if (str_strcmp(command, "cpuinfo") == 0) {
        dogeio_text_println(cpuid());
        handled = 0;
    }
    else if (str_strcmp(command, "fetch") == 0) {
        system_fetch();
        handled = 0;
    }
    else if (str_strcmp(command, "date") == 0) {
        dogeio_text_print(date_get());
        dogeio_text_print(" ");
        dogeio_text_println(time_get());
        handled = 0;
    }
    else if (str_strcmp(command, "time") == 0) {
        dogeio_text_println(time_get());
        handled = 0;
    }
    else if ((arg = get_cmd_arg(command, "edit")) != NULL) {
        if (str_strlen(arg) == 0) {
            dogeio_text_println("Error: no filename specified :(");
            handled = -1;
        } 
        
        else if (str_strcmp(arg, "--help") == 0) {
            dogeio_text_println("Dogeedit v2.1");
            dogeio_text_println("edit <file>");
            handled = 0;
        }

        else if (str_strcmp(arg, "--version") == 0) {
            dogeio_text_println("Dogeedit v2.1");
            handled = 0;
        }

        else {
            if (!fs_exists((char*)arg)) {
                fs_create((char*)arg);
            }
            system_editor((char*)arg);
            handled = 0;
        }
    }

    else if (str_startswith(command, "pci")) {
        dogeio_text_println("ADDR      IDENTITY DESCRIPTION & [VENDOR:DEVICE ID]");
        dogeio_text_println("---------------------------------------------------------");

        for (uint16_t bus = 0; bus < 256; bus++) {
            for (uint8_t slot = 0; slot < 32; slot++) {
                for (uint8_t func = 0; func < 8; func++) {
                    
                    uint16_t vendor_id = pci_read_16((uint8_t)bus, slot, func, 0x00);
                    
                    if (vendor_id == 0xFFFF) {
                        if (func == 0) break;
                        continue;
                    }

                    uint16_t device_id = pci_read_16((uint8_t)bus, slot, func, 0x02);
                    uint8_t class_code = pci_read_8((uint8_t)bus, slot, func, 0x0B);

                    dogeio_print_hex8((uint8_t)bus);
                    dogeio_text_print(":");
                    dogeio_print_hex8(slot);
                    dogeio_text_print(".");
                    
                    char f_str[2] = { (char)('0' + func), '\0' };
                    dogeio_text_print(f_str);
                    dogeio_text_print("    ");

                    dogeio_text_print(pci_class_to_name(class_code));
                    
                    dogeio_text_print(" [");
                    dogeio_print_hex16(vendor_id);
                    dogeio_text_print(":");
                    dogeio_print_hex16(device_id);
                    dogeio_text_println("]");
                }
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "settings") == 0) {
        system_settings();
        handled = 0;
    }

    else if (str_startswith(command, "run")) {
        char* target = command + 4;
        system_run_bin(target, MAX_FLAT_BINARY_SIZE);
        handled = 0;
    }

    else if (str_strcmp(command, "tab") == 0) {
        sys_switch_terminal();
        handled = 0;
    }
    
    else if (str_startswith(command, "calc")) {
        char* args = command + 5; 
    
        if (args != NULL && *args != '\0') {
            int result = util_calc(args);
            char result_str[64]; 
            str_itoa(result, result_str);
            
            dogeio_text_println(result_str);
        } else {
            dogeio_text_println("Usage: calc <expression> (e.g., calc 12+6-2)");
        }
        handled = 0;
    }

    else if (str_startswith(command, "hexdump")) {
        char* filename = command + 7;
        if (util_hexdump(filename) == -1) {
            dogeio_text_println("Error: file doesn't exist :(");
            handled = -1;
        } else {
            handled = 0;
        }
    }

    else if (str_strcmp(command, "bash") == 0) {
        system_bash();
        handled = 0;
    }

    if (handled == 1) {
        int app_result = run_app_command(command);
        if (app_result != 1) {
            handled = app_result;
        }
    }

    if (handled == 0 || handled == -1) {
        char hist_path[128];
        get_history_path(hist_path);

        int bytes_read = fs_read(hist_path, buffer, sizeof(buffer) - 512);
        if (bytes_read < 0) {
            bytes_read = 0;
        }
        buffer[bytes_read] = '\0';
        str_strcat(buffer, command);
        str_strcat(buffer, "\n");
        fs_write(hist_path, buffer);
    }

    if (handled == 1) {
        dogeio_text_print(command);
        dogeio_text_println(": command not found :(");
    }

    return handled;
}

void system_dogeshell(void) {
    char input[256];
    int status = 0;

    str_strcpy(user, "/users/");
    str_strcat(user, current_user);

    char hist_path[128];
    get_history_path(hist_path);

    if (!fs_exists(hist_path)) {
        fs_create(hist_path);
    }

    fs_chdir(user);

    while (true) {
        dogeio_text_color_change(0xFF00FF00);
        dogeio_text_print(current_user);
        dogeio_text_color_change(saved_color);
        dogeio_text_print(" (");

        char home_path[128] = {0};
        str_strcpy(home_path, "/users/");
        str_strcat(home_path, current_user);

        char* current_dir = fs_dirname();

        if (str_strcmp(current_dir, home_path) == 0) {
            dogeio_text_print("~");
        } else {
            dogeio_text_print(current_dir);
        }

        dogeio_text_print(") ");

        if (status != 0) {
            static char code_buffer[16];
            str_itoa(status, code_buffer);
            dogeio_text_color_change(0xFFFF0000);
            dogeio_text_print("[");
            dogeio_text_print(code_buffer);
            dogeio_text_print("] ");
        }

        dogeio_text_color_change(saved_color);
        dogeio_text_input("> ", input, sizeof(input));

        status = system_dogeshell_ex(input);
    }
}
