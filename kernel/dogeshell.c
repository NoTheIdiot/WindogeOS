// include files
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

void doge_shell(uint32_t magic, multiboot_info_t* mbi) {
    (void)magic;  // suppress unused parameter warning cuz gcc hates me

    char uptime_buffer[64];
    char command[64];
    char cpu_buffer[64];

    while (1) {
        dogeio_print("windoge~# ");
        dogeio_input(command, 64, DOGE_COLOR);

        if (command[0] == '\0') continue;

        int handled = 0;

        if (string_startswith(command, "print")) {
            char *args = command + 5;
            if (*args == ' ') args++;
            dogeio_println(args);
            handled = 1;

        } else if (string_startswith(command, "bark")) {
            char *args = command + 4;
            if (*args == ' ') args++;
            dogeio_println(args);
            handled = 1;

        } else if (string_startswith(command, "halt")) {
            dogeio_clear_screen();
            dogeio_print("Such halting CPU, mauch goodbye.\n");
            while (1) {__asm__("hlt");}

        } else if (string_startswith(command, "ver") || string_startswith(command, "such-version")) {
            dogeio_println(such_windoge_version);
            handled = 1;

        } else if (string_startswith(command, "clear")) {
            dogeio_clear_screen();
            handled = 1;

        } else if (string_strcmp(command, "date") == 0) {
            time_update_time();
            time_show();
            dogeio_println("");
            handled = 1;

        } else if (string_strcmp(command, "time") == 0) {
            time_update_time();
            time_show();
            dogeio_print("\n");
            handled = 1;

        } else if (string_startswith(command, "sysinfo")) {
            uint32_t ram_amount = info_get_ram_amount(mbi);
            char ram_amount_string[64];
            string_itoa(ram_amount, ram_amount_string);
            time_update_time();

            dogeio_print("windoge.human\n");
            dogeio_print("-----------------------\n");
            dogeio_print("Version: ");
            dogeio_println(such_windoge_version);
            dogeio_print("CPU: ");
            info_get_cpu_name(cpu_buffer);
            dogeio_print(cpu_buffer);
            dogeio_print("\n");
            dogeio_print("Usable RAM: ");
            dogeio_print(ram_amount_string);
            dogeio_print("MB");
            dogeio_print("\n");
            dogeio_print("Time: ");
            time_show();
            dogeio_print("\n");
            dogeio_print("Boot Time: ");
            dogeio_print(uptime_buffer);
            dogeio_print("\n");
            handled = 1;
        }

        if (string_startswith(command, "dir") || string_startswith(command, "listfiles")) {
            file_list_files();
            handled = 1;
        }

        if (string_startswith(command, "readfile")) {
            char *arg = command + 8;
            if (*arg == ' ') arg++;
            if (*arg != '\0') {
                char **file = file_find_by_name(arg);
                if (file != NULL) {
                    file_read_file(file);
                } else {
                    dogeio_print("file such not found, pls use dir.\n");
                }
            } else {
                dogeio_print("usage: readfile <filename>\n");
            }
            handled = 1;

        } else if (string_startswith(command, "read")) {
            char *arg = command + 4;
            if (*arg == ' ') arg++;
            if (*arg != '\0') {
                char **file = file_find_by_name(arg);
                if (file != NULL) {
                    file_read_file(file);
                } else {
                    dogeio_print("file not found, pls use dir\n");
                }
            } else {
                dogeio_print("usage: read <filename>\n");
            }
            handled = 1;
        }

        if (!handled) {
            dogeio_print("command not found.\n");
        }
    }
}