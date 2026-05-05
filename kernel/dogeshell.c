// include files
#include <stdint.h>
#include <stddef.h>
#include "../helper/dogeio.c"
#include "../helper/string.h"
#include "../helper/ports.h"
#include "../helper/time.h"
#include "consts.h"
#include "multiboot.h"
#include "info.h"


// check everything about the multiboot header
void doge_shell() {
    char command[64];
    char print_input[64];
    char cpu_buffer[64];

    while (1) {
        dogeio_print("windoge~# ");
        dogeio_input(command, 64, DOGE_COLOR);

        if (string_strcmp(command, "print") == 0 || string_strcmp(command, "bark") == 0) {
            dogeio_print("print> ");
            dogeio_input(print_input, 64, DOGE_COLOR);
            dogeio_print(print_input);
            dogeio_print("\n");

        } else if (string_strcmp(command, "shutdown") == 0) {
            dogeio_clear_screen();
            dogeio_print("Such shutdown, mauch goodbye.\n");
            while (1) {__asm__("hlt");}
        
        } else if (string_strcmp(command, "ver") == 0 || string_strcmp(command, "such-version") == 0) {
            dogeio_println(such_windoge_version);

        } else if (string_strcmp(command, "clear") == 0) {
            dogeio_clear_screen();

        } else if (string_strcmp(command, "date") == 0 || string_strcmp(command, "time") == 0) {
            time_update_time();
            time_show();
            dogeio_print("\n");
                
        } else if (string_strcmp(command, "sysinfo") == 0) {
            uint32_t ram_amount = dogeio_get_total_ram(mbi);
            char ram_amount_string[64];
            string_itoa(ram_amount, ram_amount_string);
            time_update_time();

            dogeio_print("windoge.human\n");
            dogeio_print("-----------------------\n");
            dogeio_print("Version: ");
            dogeio_println(such_windoge_version);
            dogeio_print("CPU: ");
            dogeio_get_cpu_name(cpu_buffer);
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

        } else {
            dogeio_print("command not found.\n");
        }
    }
}
