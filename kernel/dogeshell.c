// include files
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <dogeio.h>
#include <time.h>
#include <bool.h>
#include <info.h>
#include <file.h>

// extern some files and variables for version, boot time, 
// and the system information function.
extern char* such_windoge_version;
extern char* such_windoge_version_short;
extern char* boot_time;
extern void system_sysinfo();

// the help command array, since you can't use multiine strings like
// on python.
char* command_help[] {
    "print/bark [message]         | prints a message",
    "clear                        | clears the display",
    "time/date                    | prints the time",
    "sysinfo                      | prints your system information",
    "dir                          | list files",
    "read                         | reads and output a file",
    "help                         | prints this help message",
    "history                      | prints your shell history"
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

// execute a command
void dogeshell_execute