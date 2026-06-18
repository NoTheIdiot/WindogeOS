// include files
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <dogeio.h>
#include <time.h>
#include <bool.h>
#include "info.h"
#include <consts.h>
#include <file.h>

// extern some files and variables for version, boot time, 
// and the system information function.
extern char* such_windoge_version;
extern char* such_windoge_version_short;
extern char* boot_time;
extern void friendly_mode();
extern void system_sysinfo();

char* command_help[] = {
    "print/bark [message]         | prints a message",
    "clear                        | clears the display",
    "time/date                    | prints the time",
    "sysinfo                      | prints your system information",
    "dir                          | list files",
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
    // splits the command using spaces till it's \0
    while (*arg == ' ' && *arg != '\0') {
        arg++;
    }
    return (*arg == '\0') ? NULL : arg;
}

/******************************************************
 * SHELL STUFF
 */

void dogeshell_execute(char* command) {
    // this to check if the command exists
    int handled = 0;

    int len = string_strlen(command);
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
    if (string_startswith(command, "print")) {
        char* argument = shell_get_arg(command, 5);
        dogeio_println(argument ? argument : "");
        handled = 1;
    } else if (string_startswith(command, "bark")) {
        char* argument = shell_get_arg(command, 4);
        dogeio_println(argument ? argument : "");
        handled = 1;
    }

    /***************************************************
     * FILE MANIPULATION
     ***************************************************/
    if (string_startswith(command, "readfile")) {
        char* argument = shell_get_arg(command, 8);
        
        if (!argument) {
            dogeio_println("usage: readfile [filename]");
        } else {
            char name[9];
            char ext[4];
            int ni = 0;
            int ei = 0;
            int dot_found = 0;

            for (int i = 0; i < 8; i++) name[i] = ' ';
            name[8] = '\0';
            for (int i = 0; i < 3; i++) ext[i] = ' ';
            ext[3] = '\0';

            while (argument[ni] != '\0' && argument[ni] != '.' && ni < 8) {
                name[ni] = argument[ni];
                ni++;
            }
            name[ni] = '\0';

            int scan = 0;
            while (argument[scan] != '\0') {
                if (argument[scan] == '.') {
                    dot_found = scan + 1;
                    break;
                }
                scan++;
            }

            if (dot_found > 0) {
                while (argument[dot_found] != '\0' && ei < 3) {
                    ext[ei] = argument[dot_found];
                    ei++;
                    dot_found++;
                }
                ext[ei] = '\0';
            }

            for (int i = 0; i < 4096; i++) {
                shell_file_buffer[i] = '\0';
            }

            int read_status = fat32_read_file(name, ext, shell_file_buffer);
            if (read_status != -1) {
                dogeio_println((char*)shell_file_buffer);
            } else {
                dogeio_print("error: could not read ");
                dogeio_println(argument);
            }
        }
        handled = 1;
    } else if (string_strcmp(command, "dir") == 0) {
        fat32_list_directory("/");
        handled = 1;
    }

    /*****************************************************
     * INFORMATION SECTION
     *****************************************************/
    if (string_strcmp(command, "time") == 0 || string_strcmp(command, "date") == 0) {
        time_update_time();
        time_show();
        dogeio_print("\n");
        handled = 1;
    } else if (string_strcmp(command, "sysinfo") == 0) {
        system_sysinfo(); 
        handled = 1;
    }

    /****************************************************
     * BASIC UTILITIES
     ***************************************************/
    if (string_strcmp(command, "help") == 0) {
        for (int i = 0; i < 9; i++) {
            dogeio_println(command_help[i]);
        }
        handled = 1;
    } else if (string_strcmp(command, "halt") == 0) {
        dogeio_clear_screen();
        dogeio_println("Halting such CPU, very goodbye.");
        while (true) { __asm__ volatile ("hlt"); }
    } else if (string_strcmp(command, "clear") == 0) {
        dogeio_clear_screen();
        handled = 1;
    }

    if (!handled) {
        dogeio_print(command);
        dogeio_println(": command not found");
    }
     
    if (string_strcmp(command, "history") != 0 && string_strcmp(command, "help") != 0) {
        string_strncpy(dogeshell_history[dogeshell_history_starter], command, 127);
        dogeshell_history[dogeshell_history_starter][127] = '\0';
        
        dogeshell_history_starter++;
        if (dogeshell_history_count < 32) {
            dogeshell_history_count++;
        }
        
        if (dogeshell_history_starter >= 32) {
            dogeshell_history_starter = 0;
        }
    }
}

void doge_shell(int can_exit) {
    char command_buffer[128]; 

    while (true) {
        dogeio_print("wow (root) > ");
        dogeio_input(command_buffer, 128, LIGHT_BROWN);
        if (string_strcmp(command_buffer, "exit") == 0) {
            if (can_exit == 1) return;
            else continue;
        } else if (string_strcmp(command_buffer, "friendly") == 0) {
            if (can_exit == 0) friendly_mode();
            else dogeio_println("friendly mode is not avalible when you are already in friendly mode.");
        } else {
            dogeshell_execute(command_buffer);
        }
    }
}