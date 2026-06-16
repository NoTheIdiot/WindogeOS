#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "dogeio.h"
#include "dogeio_vbe.h"
#include "time.h"
#include "bool.h"
#include "consts.h"
#include "file.h"
#include "info.h"
#include "multiboot.h"

extern char* such_windoge_version;
extern char* such_windoge_version_short;
extern char* boot_time;
extern void system_sysinfo(void);

char* command_help[] = {
    "print/bark         | prints a message",
    "clear              | clears the screen",
    "time/date          | prints the time",
    "sysinfo            | prints the system information of your wow system",
    "dir                | lists many files",
    "read               | outputs a file",
    "help               | shows this help message",
    "history            | shows history"
};

char dogeshell_history[32][128];
int dogeshell_history_count = 0;
int dogeshell_history_starter = 0;

static uint8_t shell_file_buffer[4096];

char* shell_get_arg(char* buffer, int command_len) {
    char* arg = buffer + command_len;
    while (*arg == ' ' && *arg != '\0') {
        arg++;
    }
    return (*arg == '\0') ? NULL : arg;
}

void dogeshell_execute(char* command_buffer) {
    int handled = 0;
    
    int len = string_strlen(command_buffer);
    while (len > 0 && (command_buffer[len - 1] == '\n' || command_buffer[len - 1] == '\r')) {
        command_buffer[len - 1] = '\0';
        len--;
    }

    if (len == 0) {
        return;
    }

    if (string_startswith(command_buffer, "print")) {
        char* arg = shell_get_arg(command_buffer, 5);
        dogeio_println(arg ? arg : "");
        handled = 1;
    } 
    else if (string_startswith(command_buffer, "bark")) {
        char* arg = shell_get_arg(command_buffer, 4);
        dogeio_println(arg ? arg : "");
        handled = 1;
    }
    else if (string_startswith(command_buffer, "read")) {
        char* arg = shell_get_arg(command_buffer, 4);
        if (!arg) {
            dogeio_println("usage: read [filename.ext]");
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

            while (arg[ni] != '\0' && arg[ni] != '.' && ni < 8) {
                name[ni] = arg[ni];
                ni++;
            }
            name[ni] = '\0';

            int scan = 0;
            while (arg[scan] != '\0') {
                if (arg[scan] == '.') {
                    dot_found = scan + 1;
                    break;
                }
                scan++;
            }

            if (dot_found > 0) {
                while (arg[dot_found] != '\0' && ei < 3) {
                    ext[ei] = arg[dot_found];
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
                dogeio_println(arg);
            }
        }
        handled = 1;
    }
    else if (string_strcmp(command_buffer, "clear") == 0) {
        dogeio_clear_screen();
        handled = 1;
    }
    else if (string_strcmp(command_buffer, "halt") == 0) {
        dogeio_clear_screen();
        dogeio_println("Halting such CPU, very goodbye.");
        while (true) { __asm__ volatile ("hlt"); }
    }
    else if (string_strcmp(command_buffer, "time") == 0 || string_strcmp(command_buffer, "date") == 0) {
        time_update_time();
        time_show();
        dogeio_print("\n");
        handled = 1;
    } 
    else if (string_strcmp(command_buffer, "sysinfo") == 0) {
        system_sysinfo(); 
        handled = 1;
    }
    else if (string_strcmp(command_buffer, "dir") == 0) {
        fat32_list_directory("/");
        handled = 1;
    } 
    else if (string_startswith(command_buffer, "wait")) {
        dogeio_println("nope im not doing this now");
        handled = 1;
    } 
    else if (string_strcmp(command_buffer, "help") == 0) {
        for (int i = 0; i < 8; i++) {
            dogeio_println(command_help[i]);
        }
        handled = 1;
    }

    if (!handled) {
        dogeio_print(command_buffer);
        dogeio_println(": command not found");
    }

    if (string_strcmp(command_buffer, "history") != 0 && string_strcmp(command_buffer, "help") != 0) {
        string_strncpy(dogeshell_history[dogeshell_history_starter], command_buffer, 127);
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

void doge_shell() {
    char command_buffer[128]; 

    while (true) {
        dogeio_print("wow (root) > ");
        dogeio_input(command_buffer, 128, LIGHT_BROWN);
        dogeshell_execute(command_buffer);
    }
}
