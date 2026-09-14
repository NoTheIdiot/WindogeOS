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

uint32_t saved_color = 0xFFCCCCCC;
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
    "edit   [file]           | edits a file",
    "genimg                  | generates a solid color image",
    "viewimg                 | views solid color image",
    "=======================================================",
};

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
        int is_root = (str_strcmp(target, "/") == 0);
    
        if (!is_root && !fs_exists(target)) {
            dogeio_text_println("Error: much folder location doesn't exist.");
            handled = -1;
        } 
        else if (str_startswith(target, "/system") != 0) {
            dogeio_text_println("Error: Much permission denied :(");
            handled = -2;
        } 
        else {
            fs_chdir(target);
            handled = 0;
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
        } else {
            if (!fs_exists((char*)arg)) {
                fs_create((char*)arg);
            }
            system_editor((char*)arg);
            handled = 0;
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

        if (str_strcmp(input, "exit") == 0) {
            return;
        }
        status = system_dogeshell_ex(input);
    }
}
