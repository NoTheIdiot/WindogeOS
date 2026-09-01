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

    if (!fs_exists(".history")) {
        fs_create(".history");
    }
    fs_write(".history", (char *)command);
}

static bool is_numeric_string(const char *str) {
    if (!str || str[0] == '\0') return false;
    while (*str == ' ' || *str == '\t') str++;
    if (*str == '\0') return false;
    while (*str != '\0' && *str != '\n' && *str != '\r') {
        if (*str < '0' || *str > '9') return false;
        str++;
    }
    return true;
}

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

    else if (str_strcmp(command, "cd") == 0 || str_startswith(command, "cd ")) {
        if (str_strlen(command) <= 3) {
            dogeio_text_println("Much Error: No folder specified.");
        } else {
            char* target = command + 3;
            if (target[0] == '\0') {
                dogeio_text_println("Much Error: No folder specified.");
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

    else if (str_strcmp(command, "mkdir") == 0 || str_startswith(command, "mkdir ")) {
        if (str_strlen(command) <= 6) {
            dogeio_text_println("Error: No directory name specified.");
        } else {
            char* dir_name = command + 6;
            if (dir_name[0] == '\0') {
                dogeio_text_println("Error: Directory name cannot be empty.");
            } else {
                int res = fs_mkdir(dir_name);
                if (res != 0) {
                    dogeio_text_println("Error: Directory creation failed.");
                }
            }
        }
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

    else if (str_startswith(command, "date")) {
        const char* t = time_get();
        if (!t || t[0] == '\0') {
            dogeio_text_println("Error: Unable to retrieve system time.");
        } else {
            dogeio_text_println(t);
        }
        handled = 0;
    }

    else if (str_startswith(command, "color")) {
        const char *arg = (str_strlen(command) >= 6) ? command + 6 : "";
        if (arg[0] == '\0') {
            dogeio_text_println("Error: No color preset specified.");
        } else if (str_strcmp(arg, "black") == 0) {
            dogeio_text_color = 0xFF000000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "white") == 0) {
            dogeio_text_color = 0xFFFFFFFF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "grey") == 0) {
            dogeio_text_color = 0xFF808080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "dark_grey") == 0) {
            dogeio_text_color = 0xFF404040;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "red") == 0) {
            dogeio_text_color = 0xFFFF0000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "green") == 0) {
            dogeio_text_color = 0xFF00FF00;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "blue") == 0) {
            dogeio_text_color = 0xFF0000FF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "yellow") == 0) {
            dogeio_text_color = 0xFFFFFF00;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "cyan") == 0) {
            dogeio_text_color = 0xFF00FFFF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "magenta") == 0) {
            dogeio_text_color = 0xFFFF00FF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "navy") == 0) {
            dogeio_text_color = 0xFF000080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "maroon") == 0) {
            dogeio_text_color = 0xFF800000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "teal") == 0) {
            dogeio_text_color = 0xFF008080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "olive") == 0) {
            dogeio_text_color = 0xFF808000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "doge_gold") == 0) {
            dogeio_text_color = 0xFFE1B857;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "doge_tan") == 0) {
            dogeio_text_color = 0xFFF4DFB1;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "gray") == 0) {
            dogeio_text_color = 0xFFCCCCCC;
            dogeio_text_clear();
        } else {
            dogeio_text_println("Error: Unknown color preset.");
        }
        handled = 0;
    }

    else if (str_strcmp(command, "cat") == 0 || str_startswith(command, "cat ")) {
        if (str_strlen(command) <= 4) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 4;
            if (filename[0] == '\0') {
                dogeio_text_println("Error: No filename specified.");
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

    else if (str_strcmp(command, "format") == 0) {
        char r_u_sure[4];
        dogeio_text_input("Are You Sure? (yes/no)\nMUCH WARNING: THIS WILL ERASE THE DISK.\n", r_u_sure, 4);
        if (str_strcmp(r_u_sure, "yes") == 0) {
            int result = fs_format();
            if (result) {
                dogeio_text_println("Formated Disk.");
            } else {
                dogeio_text_println("Not Wow: Something Went Wrong.");
            }
        } else if (str_strcmp(r_u_sure, "no") == 0) {
            dogeio_text_println("Format canceled.");
        } else {
            dogeio_text_println("Error: Invalid response. Format aborted.");
        }
        handled = 0;
    }

    else if (str_strcmp(command, "fetch") == 0) {
        system_fetch();
        handled = 0;
    }

    else if (str_strcmp(command, "whoami") == 0) {
        dogeio_text_println("wow");
        handled = 0;
    }

    else if (str_strcmp(command, "pwd") == 0) {
        dogeio_text_println(fs_dirname());
        handled = 0;
    }

    else if (str_strcmp(command, "sed") == 0 || str_startswith(command, "sed ")) {
        if (str_strlen(command) <= 4) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 4;
            if (filename[0] == '\0') {
                dogeio_text_println("Error: No filename specified.");
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

    else if (str_strcmp(command, "history") == 0) {
        if (!fs_exists(".history")) {
            dogeio_text_println("No history available.");
        } else {
            static char output_buffer[8192];
            int bytes_read = fs_read(".history", output_buffer, sizeof(output_buffer) - 1);
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

    else if (str_strcmp(command, "touch") == 0 || str_startswith(command, "touch ")) {
        if (str_strlen(command) <= 6) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 6;
            if (filename[0] == '\0') {
                dogeio_text_println("Error: No filename specified.");
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

    else if (str_strcmp(command, "rm") == 0 || str_startswith(command, "rm ")) {
        if (str_strlen(command) <= 3) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 3;
            if (filename[0] == '\0') {
                dogeio_text_println("Error: No filename specified.");
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

    else if (str_strcmp(command, "edit") == 0 || str_startswith(command, "edit ")) {
        if (str_strlen(command) <= 5) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 5;

            size_t len = str_strlen(filename);
            while (len > 0 && (filename[len - 1] == '\n' || filename[len - 1] == '\r' || filename[len - 1] == ' ')) {
                filename[len - 1] = '\0';
                len--;
            }

            if (str_strlen(filename) == 0) {
                dogeio_text_println("Error: No filename specified.");
            } else {
                if (!fs_exists(filename)) {
                    if (fs_create(filename) < 0) {
                        dogeio_text_println("Error: Could not create file for editing.");
                    } else {
                        system_editor(filename);
                    }
                } else {
                    system_editor(filename);
                }
            }
        }
        handled = 0;
    }

    else if (str_startswith(command, "viewimg")) {
        char* argument = command +8;
        system_parse_tga(argument);
        handled = 0;
    }

    else if (str_startswith(command, "genimg")) {
        char* filename = command + 7;
        if (filename[0] == '\0') {
            dogeio_text_println("Usage: genimg <filename>");
        } else {
            char red[4];
            char green[4];
            char blue[4];

            dogeio_text_input("red> ", red, 4);
            dogeio_text_input("green> ", green, 4);
            dogeio_text_input("blue> ", blue, 4);

            if (!is_numeric_string(red) || !is_numeric_string(green) || !is_numeric_string(blue)) {
                dogeio_text_println("Error: RGB values must be numeric digits (0-255).");
            } else {
                uint8_t red8 = str_to_u8(red);
                uint8_t green8 = str_to_u8(green);
                uint8_t blue8 = str_to_u8(blue);
            
                generate_tga(filename, red8, green8, blue8);
            }
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
    int status = 0;

    if (!fs_exists(".history")) {
        fs_create(".history");
    }

    while (true) {
        dogeio_text_color_change(0xFF00FF00);
        dogeio_text_print(current_user);
        dogeio_text_color_change(old);
        dogeio_text_print(":");

        char home_path[128];
        for (int i = 0; i < 128; i++) {
            home_path[i] = '\0';
        }
        
        dogeio_text_color_change(0xADD8E6);
        str_strcpy(home_path, "/users/");
        str_strcat(home_path, current_user);

        char* current_dir = fs_dirname();

        if (str_strcmp(current_dir, home_path) == 0) {
            dogeio_text_print("~");
        } else {
            dogeio_text_print(current_dir);
        }

        if (status == 1) {
            dogeio_text_color_change(0xFFFF0000);
            dogeio_text_print("[1] ");
        }

        dogeio_text_color_change(old);
        dogeio_text_input("$ ", input, 256);
        
        if (input[0] != '\0') {
            append_history(input);
        }
        
        if (str_strcmp(input, "exit") == 0) {
            return;
        }
        
        status = system_bash_ex(input);
    }
}