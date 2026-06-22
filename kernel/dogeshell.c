// include files
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <dogeio.h>
#include <time.h>
#include <bool.h> // Swapped back to your original bool header
#include "info.h"
#include <consts.h>
#include <file.h>

// extern files and variables
extern char* such_windoge_version;
extern char* such_windoge_version_short;
extern char* boot_time;
extern uint8_t terminal_color;
extern void friendly_mode();
extern void system_systeminfo();

const char* command_help[9] = {
    "print/bark [message]         | prints a message",
    "clear                        | clears the display",
    "time/date                    | prints the time",
    "sysinfo                      | prints your system information",
    "dir [folder]                 | list files",
    "readfile [filename]          | reads and output a file",
    "help                         | prints this help message",
    "history                      | prints your shell history",
    "friendly                     | starts friendly mode"
};

// array for storing shell history
char dogeshell_history[32][128];
int dogeshell_history_count = 0;
int dogeshell_history_starter = 0;

// a buffer to store files for reading
uint8_t shell_file_buffer[4096];

// get an argument for a command
char* shell_get_arg(char* buffer, int command_len) {
    char* arg = buffer + command_len;
    while (*arg == ' ' && *arg != '\0') {
        arg++;
    }
    return (*arg == '\0') ? NULL : arg;
}

// parse filenames because it's annoying
int parse_filename(const char* input, char* name, char* ext) {
    for (int i = 0; i < 8; i++) name[i] = ' ';
    for (int i = 0; i < 3; i++) ext[i] = ' ';

    int ni = 0, ei = 0;
    int seen_dot = 0;

    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == '.') {
            seen_dot = 1;
            continue;
        }

        if (!seen_dot && ni < 8) {
            name[ni++] = input[i];
        } else if (seen_dot && ei < 3) {
            ext[ei++] = input[i];
        }
    }

    return (ni > 0);
}


/******************************************************
 * SHELL STUFF
 */

void dogeshell_execute(char* command) {
    int handled = 0;
    int len = string_strlen(command);
    
    // Clean trailing newlines
    while (len > 0 && (command[len - 1] == '\n' || command[len - 1] == '\r')) {
        command[len - 1] = '\0';
        len--;
    }

    if (len == 0) {
        return;
    }

    /*****************************************************
     * BASIC SHELL OUTPUT
     *****************************************************/
    int is_print = string_startswith(command, "print");
    int is_bark  = string_startswith(command, "bark");
    
    if (is_print || is_bark) {
        char* argument = shell_get_arg(command, is_print ? 5 : 4);
        dogeio_println(argument ? argument : "");
        handled = 1;
    }

    /***************************************************
     * FILE MANIPULATION
     ***************************************************/
    else if (string_startswith(command, "readfile")) {
        char* argument = shell_get_arg(command, 8);
        
        if (!argument || string_strcmp(argument, "--help") == 0) {
            dogeio_println("Such usage: readfile [filename]");
            dogeio_println("Reads a file and outputs it into the display.");
            dogeio_println("Input must be fully capitalized, for example:");
            dogeio_println("readfile README.TXT");
        } else {
            char name[9] = "        "; 
            char ext[4]  = "   ";
            int ni = 0;
            
            while (argument[ni] != '\0' && argument[ni] != '.' && ni < 8) {
                name[ni] = argument[ni];
                ni++;
            }
            name[ni] = '\0';

            // Custom inline strchr replacement to find the dot
            char* dot = NULL;
            char* scan = argument;
            while (*scan != '\0') {
                if (*scan == '.') {
                    dot = scan;
                    break;
                }
                scan++;
            }

            if (dot) {
                dot++; // Skip the dot itself
                int ei = 0;
                while (*dot != '\0' && ei < 3) {
                    ext[ei++] = *dot++;
                }
                ext[ei] = '\0';
            }

            // Custom inline memset replacement
            for (int i = 0; i < 4096; i++) {
                shell_file_buffer[i] = '\0';
            }

            if (fat32_read_file("/", name, ext, shell_file_buffer) != -1) {
                dogeio_println((char*)shell_file_buffer);
            } else {
                dogeio_print("error: could not read ");
                dogeio_println(argument);
            }
        }
        handled = 1;

    } else if (string_startswith(command, "dir")) {
        char* argument = shell_get_arg(command, 3);
        
        if (!argument) {
            fat32_list_directory("/");
        } else if (string_strcmp(argument, "--help") == 0) {
            dogeio_println("wow usage: dir [directory]");
            dogeio_println("this command can be used without a argument input.");
        } else {
            fat32_list_directory(argument);
        }
        handled = 1;
    
    } else if (string_startswith(command, "createfile")) {
        char* argument = shell_get_arg(command, 10);

        if (!argument || string_strcmp(argument, "--help") == 0) {
            dogeio_println("usage: createfile [filename]");
            dogeio_println("creates a file.");
        } else {
            char name[8], ext[3];

            if (!parse_filename(argument, name, ext)) {
                dogeio_println("invalid filename");
            } else {
                int res = fat32_create_file("/", name, ext);
                if (res == 0) {
                    dogeio_println("file created");
                } else {
                    dogeio_println("error creating file");
                }
            }
        }
        handled = 1;
    } else if (string_startswith(command, "deletefile")) {
        char* argument = shell_get_arg(command, 10);

        if (!argument || string_strcmp(argument, "--help") == 0) {
            dogeio_println("usage: deletefile [filename]");
            dogeio_println("deletes a file.");
        } else {
            char name[8], ext[3];

            if (!parse_filename(argument, name, ext)) {
                dogeio_println("invalid filename");
            } else {
                int res = fat32_delete_file("/", name, ext);

                if (res == 0) {
                    dogeio_print("file ");
                    dogeio_print(argument);
                    dogeio_println(" such deleted.");
                } else {
                    dogeio_println("delete failed, you should try again.");
                }
            }
        }
        handled = 1;
    } else if (string_startswith(command, "writefile")) {
    char* argument = shell_get_arg(command, 9);

    if (!argument || string_strcmp(argument, "--help") == 0) {
        dogeio_println("usage: writefile [filename] [text]");
        dogeio_println("writes a piece of text of text to a file, though");
        dogeio_println("it erases a file's data and will be fixed soon.");
    } else {
        // split filename and content
        char* space = argument;
        while (*space != '\0' && *space != ' ') space++;

        if (*space == '\0') {
            dogeio_println("missing text");
        } else {
                *space = '\0';
                char* text = space + 1;

                char name[8], ext[3];

                if (!parse_filename(argument, name, ext)) {
                    dogeio_println("invalid filename");
                } else {
                    int res = fat32_write_file("/", name, ext, (uint8_t*)text, string_strlen(text));

                    if (res == 0) {
                        dogeio_println("write success");
                    } else {
                        dogeio_println("write failed");
                    }
                }
            }
        }
        handled = 1;
    }




    /*****************************************************
     * INFORMATION SECTION
     *****************************************************/
    else if (string_strcmp(command, "time") == 0 || string_strcmp(command, "date") == 0) {
        time_update_time();
        time_show();
        dogeio_print("\n");
        handled = 1;
    } else if (string_startswith(command, "time ") || string_startswith(command, "date ")) {
        char* argument = shell_get_arg(command, 4);
        if (argument && string_strcmp(argument, "--help") == 0) {
            dogeio_println("Much usage: time/date");
        }
        handled = 1;
    } else if (string_strcmp(command, "sysinfo") == 0) {
        if (!shell_get_arg(command, 7)) {
            system_systeminfo(); 
            handled = 1;
        }
    }

    /****************************************************
     * BASIC UTILITIES
     ***************************************************/
    else if (string_strcmp(command, "help") == 0) {
        for (int i = 0; i < 9; i++) {
            dogeio_println((char*)command_help[i]);
        }
        handled = 1;
    } else if (string_strcmp(command, "halt") == 0) {
        dogeio_clear_screen();
        dogeio_println("Halting such CPU, very goodbye.");
        while (1) { __asm__ volatile ("hlt"); }
    } else if (string_strcmp(command, "clear") == 0) {
        dogeio_clear_screen();
        handled = 1;
    } else if (string_strcmp(command, "history") == 0) {
        int index = (dogeshell_history_count == 32) ? dogeshell_history_starter : 0;
        for (int i = 0; i < dogeshell_history_count; i++) {
            dogeio_println(dogeshell_history[index]);
            index = (index + 1) % 32;
        }
        handled = 1;
    }

    /***********************************************************
     * FUNNI TERMINAL STUFF
     ***********************************************************/
    else if (string_startswith(command, "color")) {
        char* argument = shell_get_arg(command, 5);

        if (!argument || string_strcmp(argument, "--help") == 0) {
            dogeio_println("Many usage: color [color_name]");
            dogeio_println("Changes the terminal color.");
            dogeio_println("Color options:");
            dogeio_println("black, blue, green, red, brown, doge, white");
        } else {
            int valid_color = 1;
            if (string_strcmp(argument, "white") == 0)          terminal_color = WHITE;
            else if (string_strcmp(argument, "blue") == 0)      terminal_color = BLUE;
            else if (string_strcmp(argument, "green") == 0)     terminal_color = GREEN;
            else if (string_strcmp(argument, "red") == 0)       terminal_color = RED;
            else if (string_strcmp(argument, "brown") == 0)     terminal_color = BROWN;
            else if (string_strcmp(argument, "doge") == 0)      terminal_color = DOGE_COLOR;
            else if (string_strcmp(argument, "cyan") == 0)      terminal_color = CYAN;
            else if (string_strcmp(argument, "lightblue") == 0) terminal_color = LIGHT_BLUE;
            else if (string_strcmp(argument, "lightgreen") == 0)terminal_color = LIGHT_GREEN;
            else if (string_strcmp(argument, "lightgrey") == 0) terminal_color = LIGHT_GREY;
            else if (string_strcmp(argument, "lightmangenta") == 0)terminal_color = LIGHT_MAGENTA;
            else valid_color = 0;

            if (valid_color) {
                dogeio_clear_screen();
            } else {
                dogeio_print("color ");
                dogeio_print(argument);
                dogeio_println(" not exist.");
            }
        }
        handled = 1;
    }

    // Command not found fall-through
    if (!handled) {
        dogeio_print(command);
        dogeio_println(": command not found");
    }
     
    // Save history using secure index wrap-around tracking
    if (string_strcmp(command, "history") != 0 && string_strcmp(command, "help") != 0 && len > 0) {
        string_strncpy(dogeshell_history[dogeshell_history_starter], command, 127);
        dogeshell_history[dogeshell_history_starter][127] = '\0';
        
        dogeshell_history_starter = (dogeshell_history_starter + 1) % 32;
        if (dogeshell_history_count < 32) {
            dogeshell_history_count++;
        }
    }
}

void doge_shell(int can_exit) {
    char command_buffer[128]; 

    while (1) {
        dogeio_print("wow (root) > ");
        dogeio_input(command_buffer, 128, terminal_color);
        
        if (string_strcmp(command_buffer, "exit") == 0) {
            if (can_exit == 1) return;
            else continue;
        } else if (string_strcmp(command_buffer, "friendly") == 0) {
            if (can_exit == 0) {
                friendly_mode();
            } else {
                dogeio_println("friendly mode is not available when you are already in friendly mode.");
            }
        } else {
            dogeshell_execute(command_buffer);
        }
    }
}
