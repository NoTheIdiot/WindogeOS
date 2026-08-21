#include <stdint.h>
#include <dogeio.h>
#include <basicutil.h>
#include <string.h>
#include <bool.h>
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
            int result = fs_chdir(command + 3);
            if (result == -2) {
                dogeio_text_println("Much Error: Not a Folder.");
            } else if (result == -1) {
                dogeio_text_println("Such Error: Folder not existing :(");
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "mkdir") == 0 || str_startswith(command, "mkdir ")) {
        if (str_strlen(command) <= 6) {
            dogeio_text_println("Error: No directory name specified.");
        } else {
            fs_mkdir(command + 6);
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
        dogeio_text_println(time_get());
        handled = 0;
    }

    else if (str_startswith(command, "color")) {
        const char *arg = (str_strlen(command) >= 6) ? command + 6 : "";
        if (str_strcmp(arg, "black") == 0) {
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
            dogeio_text_println("unknown colorpreset.");
        }
        handled = 0;
    }

    else if (str_strcmp(command, "cat") == 0 || str_startswith(command, "cat ")) {
        if (str_strlen(command) <= 4) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 4;
            static char output_buffer[8192];

            int bytes_read = fs_read(filename, output_buffer, sizeof(output_buffer) - 1);
            if (bytes_read < 0) {
                dogeio_text_println("Error: unable to read file.");
            } else {
                output_buffer[bytes_read] = '\0';
                dogeio_text_print(output_buffer);
                dogeio_text_println("");
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
        } else {
            dogeio_text_println("Format skipped.");
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
        dogeio_text_println("root (/)");
        handled = 0;
    }

    else if (str_strcmp(command, "sed") == 0 || str_startswith(command, "sed ")) {
        if (str_strlen(command) <= 4) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 4;
            static char text[256];
            
            dogeio_text_input("text> ", text, 256);
            
            int result = fs_write(filename, text);
            if (result == 0) {
                dogeio_text_println("write ok");
            } else if (result == -2) {
                dogeio_text_println("Error: file not found.");
            } else {
                dogeio_text_println("Not Wow: Something went wrong.");
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "history") == 0) {
        static char output_buffer[8192];
        int bytes_read = fs_read(".history", output_buffer, sizeof(output_buffer) - 1);
        if (bytes_read < 0) {
            dogeio_text_println("No history available.");
        } else {
            output_buffer[bytes_read] = '\0';
            dogeio_text_print(output_buffer);
            dogeio_text_println("");
        }
        handled = 0;
    }

    else if (str_strcmp(command, "touch") == 0 || str_startswith(command, "touch ")) {
        if (str_strlen(command) <= 6) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 6;
            int result = fs_create(filename);

            if (result < 0) {
                dogeio_text_println("Not Wow: Failed to create file.");
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "rm") == 0 || str_startswith(command, "rm ")) {
        if (str_strlen(command) <= 3) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            char* filename = command + 3;

            int result = fs_delete(filename);
            if (!result) {
                dogeio_text_println("Not Wow: File Not Found.");
            } else if (result == -1) {
                dogeio_text_println("Doge Sad: Something went wrong!");
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "shutdown") == 0) {
        dogeio_text_clear_raw();
        dogeio_text_println("Such shutdown, very goodbye.");
        halt();
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
                    fs_create(filename);
                }
                editor(filename);
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

    if (!fs_exists(".history")) {
        fs_create(".history");
    }

    while (true) {
        for (int i = 0; i < 256; i++) {
            input[i] = '\0';
        }

        dogeio_text_color_change(0x0000FF00);
        dogeio_text_print("wow");
        dogeio_text_color_change(0x000000FF);
        dogeio_text_print(":");
        char* dir = fs_dirname();

        if (str_strcmp(dir, "root") == 0) {
            dir = "";
        }

        dogeio_text_print(dir);
        dogeio_text_print("/");

        dogeio_text_color_change(0xFFFFFFFF);
        dogeio_text_input("$ ", input, 256);

        if (input[0] != '\0') {
            append_history(input);
        }

        if (str_strcmp(input, "exit") == 0) {
            return;
        }
        system_bash_ex(input);
    }
}