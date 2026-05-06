#include <stddef.h>
#include <stdint.h>
#include "../helper/dogeio.h"
#include "../helper/dogeio_graphics.h"
#include "../helper/vbe.h"
#include "../helper/string.h"
#include "../helper/ports.h"
#include "../helper/time.h"
#include "consts.h"
#include "multiboot.h"
#include "info.h"
#include "file.h"

extern char* such_windoge_version;
extern char* such_windoge_version_data;
extern void doge_script(char* file[]);


void doge_shell(uint32_t magic, multiboot_info_t* mbi) {
    (void)magic;
    char uptime_buffer[64];
    char command[128]; 
    char cpu_buffer[64];

    while (1) {
        dogeio_print("windoge~# ");
        dogeio_input(command, 128, DOGE_COLOR);
        if (command[0] == '\0') continue;

        int handled = 0;

        if (string_startswith(command, "print ")) {
            dogeio_println(command + 6);
            handled = 1;
            
        } else if (string_startswith(command, "bark ")) {
            dogeio_println(command + 5);
            handled = 1;

        } else if (string_startswith(command, "halt")) {
            dogeio_clear_screen();
            dogeio_print("Such halting CPU, mauch goodbye.\n");
            while (1) { __asm__("hlt"); }

        } else if (string_startswith(command, "ver") || string_startswith(command, "such-version")) {
            dogeio_println(such_windoge_version);
            handled = 1;

        } else if (string_startswith(command, "clear")) {
            dogeio_clear_screen();
            handled = 1;

        } else if (string_strcmp(command, "date") == 0 || string_strcmp(command, "time") == 0) {
            time_update_time();
            time_show();
            dogeio_print("\n");
            handled = 1;

        } else if (string_startswith(command, "sysinfo")) {
            uint32_t ram_amount = info_get_ram_amount(mbi);
            char ram_amount_string[64];
            string_itoa(ram_amount, ram_amount_string);
            time_update_time();
            dogeio_print("windoge.human\n-----------------------\nVersion: ");
            dogeio_println(such_windoge_version);
            dogeio_print("CPU: ");
            info_get_cpu_name(cpu_buffer);
            dogeio_print(cpu_buffer);
            dogeio_print("\nUsable RAM: ");
            dogeio_print(ram_amount_string);
            dogeio_print("MB\nTime: ");
            time_show();
            dogeio_print("\nBoot Time: ");
            dogeio_print(uptime_buffer);
            dogeio_print("\n");
            handled = 1;

        } else if (string_startswith(command, "dir") || string_startswith(command, "listfiles")) {
            file_list_files();
            handled = 1;

        } else if (string_startswith(command, "dogescript ")) {
            char **file = file_find_by_name(command + 11);
            if (file) doge_script(file);
            else dogeio_print("file not found\n");
            handled = 1;

        } else if (string_startswith(command, "readfile ") || string_startswith(command, "read ")) {
            char *name = string_startswith(command, "readfile ") ? command + 9 : command + 5;
            char **file = file_find_by_name(name);
            if (file) file_read_file(file);
            else dogeio_print("file not found\n");
            handled = 1;

        } else if (string_startswith(command, "deleteline ")) {
            char *filename = command + 11;
            char *line_ptr = filename;
            while (*line_ptr != ' ' && *line_ptr != '\0') line_ptr++;
            if (*line_ptr == ' ') {
                *line_ptr = '\0';
                line_ptr++;
                char **f = file_find_by_name(filename);
                if (f) file_delete_line(f, string_atoi(line_ptr));
            }
            handled = 1;

        }  else if (string_startswith(command, "writefile ") || string_startswith(command, "write ")) {
            char *arg = string_startswith(command, "writefile ") ? command + 10 : command + 6;
            char *filename = arg;
            char *cursor = arg;

            while (*cursor != '\0' && *cursor != ' ') {
                cursor++;
            }

            if (*cursor == ' ') {
                *cursor = '\0';
                cursor++;
                while (*cursor == ' ') cursor++;

                if (*cursor == '"') {
                    cursor++;
                    char *content = cursor;
                    while (*cursor != '\0' && *cursor != '"') {
                        cursor++;
                    }

                    if (*cursor == '"') {
                        *cursor = '\0';
                        char **file = file_find_by_name(filename);
                        if (file != NULL) {
                            file_write_file(file, content);
                        } else {
                            dogeio_print("file not found\n");
                        }
                    } else {
                        dogeio_print("error: missing closing quote\n");
                    }
                } else {
                    dogeio_print("usage: write <file> \"content\"\n");
               }
            }
            handled = 1;
 
        } else if (string_startswith(command, "dogescript")) {
            char *arg = command + 10;
            if (*arg == ' ') arg++;
            if (*arg != '\0') {
                char **file = file_find_by_name(arg);
                if (file != NULL) {
                    doge_script(file);
                } else {
                    dogeio_print("file not found, pls use dir\n");
                }

            } else {
                dogeio_print("usage: dogescript <filename>\n");
            }
            handled = 1;
        }

        if (!handled && command[0] != '\0') {
            dogeio_print("Much error: ");
            dogeio_print(command);
            dogeio_print(" is not such command.\n");
        }
    }
}
