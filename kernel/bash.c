#include <stdint.h>
#include <dogeio.h>
#include <basicutil.h>
#include <string.h>
#include <bool.h>
#include <image.h>
#include <time.h>
#include <system.h>

static void append_history(const char* command) {
    if (!command || command[0] == '\0') {
        return;
    }

    char history_path[256];
    str_strcpy(history_path, "/users/");
    str_strcat(history_path, current_user);
    str_strcat(history_path, "/.history");

    if (!fs_exists(history_path)) {
        fs_create(history_path);
    }
    fs_write(history_path, (char *)command);
}

static bool is_path_allowed(const char* path) {
    if (!path) return false;
    if (str_startswith(path, "/system")) {
        return false;
    }
    if (str_startswith(path, "/users/")) {
        char allowed_prefix[128];
        str_strcpy(allowed_prefix, "/users/");
        str_strcat(allowed_prefix, current_user);
        if (!str_startswith(path, allowed_prefix)) {
            return false;
        }
    }
    return true;
}

extern uint32_t saved_color;

int system_bash_ex(char* command) {
    int handled = 1; 

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    if (str_strcmp(command, "echo") == 0 || str_startswith(command, "echo ")) {
        size_t len = str_strlen(command);
        if (len >= 5 && command[4] == ' ') {
            dogeio_text_println(command + 5);
        } else {
            dogeio_text_println(""); 
        }
        handled = 0;
    }

    else if (str_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 0;
    }

    else if (str_strcmp(command, "pwd") == 0) {
        dogeio_text_println(fs_dirname());
        handled = 0;
    }

    else if (str_strcmp(command, "ls") == 0) {
        fs_list_dir(0);
        handled = 0;
    }
    else if (str_strcmp(command, "ls -a") == 0 || str_strcmp(command, "ls -all") == 0) {
        fs_list_dir(1);
        handled = 0;
    }

    else if (str_strcmp(command, "cd") == 0 || str_startswith(command, "cd ")) {
        if (str_strlen(command) <= 3) {
            dogeio_text_println("Much Error: No folder specified.");
        } else {
            char* target = command + 3;
            if (!is_path_allowed(target)) {
                dogeio_text_println("Much Error: Access denied to path.");
            } else {
                int result = fs_chdir(target);
                if (result == -2) {
                    dogeio_text_println("Much Error: Not a Folder.");
                } else if (result == -1) {
                    dogeio_text_println("Such Error: Folder not existing :(");
                } else if (result != 1) {
                    dogeio_text_println("Much Error: Could not change directory.");
                }
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "touch") == 0 || str_startswith(command, "touch ")) {
        if (str_strlen(command) <= 6) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 6;
            if (!is_path_allowed(filename)) {
                dogeio_text_println("Error: Access denied.");
            } else if (fs_exists(filename)) {
                dogeio_text_println("Error: File already exists.");
            } else {
                int result = fs_create(filename);
                if (result < 0) {
                    dogeio_text_println("Not Wow: Failed to create file.");
                }
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "mkdir") == 0 || str_startswith(command, "mkdir ")) {
        if (str_strlen(command) <= 6) {
            dogeio_text_println("Error: No directory name specified.");
        } else {
            char* dir_name = command + 6;
            if (!is_path_allowed(dir_name)) {
                dogeio_text_println("Error: Access denied.");
            } else {
                int res = fs_mkdir(dir_name);
                if (res != 0) {
                    dogeio_text_println("Error: Directory creation failed.");
                }
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "cat") == 0 || str_startswith(command, "cat ")) {
        if (str_strlen(command) <= 4) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 4;
            if (!is_path_allowed(filename)) {
                dogeio_text_println("Error: Access denied.");
            } else if (!fs_exists(filename)) {
                dogeio_text_println("Error: File does not exist.");
            } else {
                static char output_buffer[8192];
                int bytes_read = fs_read(filename, output_buffer, sizeof(output_buffer) - 1);
                if (bytes_read < 0) {
                    dogeio_text_println("Error: Unable to read file.");
                } else {
                    output_buffer[bytes_read] = '\0';
                    dogeio_text_print(output_buffer);
                    dogeio_text_println("");
                }
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "sed") == 0 || str_startswith(command, "sed ")) {
        if (str_strlen(command) <= 4) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 4;
            if (!is_path_allowed(filename)) {
                dogeio_text_println("Error: Access denied.");
            } else {
                static char text[256];
                dogeio_text_input("text> ", text, 256);
                int result = fs_write(filename, text);
                if (result == 0) {
                    dogeio_text_println("write ok");
                } else if (result == -2) {
                    dogeio_text_println("Error: File not found.");
                } else {
                    dogeio_text_println("Not Wow: Something went wrong.");
                }
            }
        }
        handled = 0;
    }

    else if (str_startswith(command, "mv ")) {
        char* args = command + 3;
        char old_name[128] = {0};
        char new_name[128] = {0};
        int i = 0;
        
        while (args[i] != ' ' && args[i] != '\0' && i < 127) {
            old_name[i] = args[i];
            i++;
        }
        old_name[i] = '\0';

        if (args[i] == ' ') {
            i++;
            int j = 0;
            while (args[i] != '\0' && j < 127) {
                new_name[j++] = args[i++];
            }
            new_name[j] = '\0';
        }

        if (old_name[0] == '\0' || new_name[0] == '\0') {
            dogeio_text_println("Usage: mv <old_path> <new_path>");
        } else if (!is_path_allowed(old_name) || !is_path_allowed(new_name)) {
            dogeio_text_println("Error: Access denied.");
        } else {
            int result = fs_rename(old_name, new_name);
            if (result != 0) {
                dogeio_text_println("Not Wow: Failed to move/rename file.");
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "rm") == 0 || str_startswith(command, "rm ")) {
        if (str_strlen(command) <= 3) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 3;
            if (!is_path_allowed(filename)) {
                dogeio_text_println("Error: Access denied.");
            } else {
                int result = fs_delete(filename);
                if (!result) {
                    dogeio_text_println("Not Wow: File Not Found.");
                } else if (result == -1) {
                    dogeio_text_println("Doge Sad: Something went wrong!");
                }
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "whoami") == 0) {
        dogeio_text_println("wow");
        handled = 0;
    }

    else if (str_strcmp(command, "ver") == 0) {
        dogeio_text_println("DogeOS Bash Compatibility Layer v1.0");
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

    else if (str_startswith(command, "date")) {
        const char* t = time_get();
        if (!t || t[0] == '\0') {
            dogeio_text_println("Error: Unable to retrieve system time.");
        } else {
            dogeio_text_println(t);
        }
        handled = 0;
    }

    else if (str_strcmp(command, "history") == 0) {
        char history_path[256];
        str_strcpy(history_path, "/users/");
        str_strcat(history_path, current_user);
        str_strcat(history_path, "/.history");

        if (!fs_exists(history_path)) {
            dogeio_text_println("No history available.");
        } else {
            static char output_buffer[8192];
            int bytes_read = fs_read(history_path, output_buffer, sizeof(output_buffer) - 1);
            if (bytes_read <= 0) {
                dogeio_text_println("No history available.");
            } else {
                output_buffer[bytes_read] = '\0';
                dogeio_text_print(output_buffer);
                dogeio_text_println("");
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "clear-history") == 0) {
        char history_path[256];
        str_strcpy(history_path, "/users/");
        str_strcat(history_path, current_user);
        str_strcat(history_path, "/.history");
        fs_delete(history_path);
        dogeio_text_println("History cleared.");
        handled = 0;
    }

    else if (str_strcmp(command, "shutdown") == 0) {
        dogeio_text_clear_raw();
        dogeio_text_println("Such shutdown, very goodbye.");
        core_shutdown();
        handled = 0;
    }

    else if (str_strcmp(command, "reboot") == 0) {
        dogeio_text_clear_raw();
        dogeio_text_println("Very reboot, much restart.");
        core_reboot();
        handled = 0;
    }

    else if (str_startswith(command, "run ")) {
        char* binary = command + 4;
        if (!is_path_allowed(binary)) {
            dogeio_text_println("Error: Access denied.");
        } else {
            system_run_exec(binary, 65536);
        }
        handled = 0;
    }

    if (handled == 1) {
        dogeio_text_print(command);
        dogeio_text_println(": command not found");
    }

    return handled;
}

void system_bash(void) {
    char input[256];
    int last_status = 0;

    while (true) {
        dogeio_text_color_change(0xFF00FF00);
        dogeio_text_print(current_user);
        dogeio_text_color_change(saved_color);
        dogeio_text_print(" ");

        char home_path[128] = {0};
        str_strcpy(home_path, "/users/");
        str_strcat(home_path, current_user);

        char* current_dir = fs_dirname();

        dogeio_text_color_change(0xADD8E6);
        if (str_strcmp(current_dir, home_path) == 0) {
            dogeio_text_print("~");
        } else {
            dogeio_text_print(current_dir);
        }

        dogeio_text_color_change(saved_color);
        
        if (last_status != 0) {
            dogeio_text_color_change(0xFFFF0000);
        } else {
            dogeio_text_color_change(0xFF00FF00);
        }
        
        dogeio_text_input(" > ", input, 256);
        dogeio_text_color_change(saved_color);

        if (input[0] != '\0') {
            append_history(input);
        }

        if (str_strcmp(input, "exit") == 0) {
            return;
        }

        last_status = system_bash_ex(input);
    }
}