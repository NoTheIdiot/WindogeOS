#include <dogeio.h>
#include <string.h>
#include "../kernel/info.h"
#include <consts.h>
#include <bool.h>

// extern everything needed
extern void friendly_mode();

// always run this FIRST since it's in the boot folder
// FIXED: Changed 'int friendly' to an integer pointer 'int *friendly'
int windoge_boot_manager() {

    char boot_choice[16];

    while (true) {
        dogeio_clear_screen();
        dogeio_println("**********************************************************************");
        dogeio_println("** WindogeOS Boot Manager V1.0");
        dogeio_println("**********************************************************************");
        dogeio_println("[1] Boot WindogeOS Normally");
        dogeio_println("[2] Boot in Friendly Mode");
        dogeio_println("[3] System Diagnostics");
        dogeio_println("[4] Halt");

        dogeio_print("> ");
        dogeio_input(boot_choice, 16, DOGE_COLOR);

        if (string_strcmp(boot_choice, "1") == 0 || string_strcmp(boot_choice, "") == 0) {
            return 0;
        } else if (string_strcmp(boot_choice, "2") == 0) {
            return 1;
        } else if (string_strcmp(boot_choice, "3") == 0) { 
            dogeio_clear_screen();
            system_systeminfo();
            dogeio_print("Press Enter to exit.");
            dogeio_input(boot_choice, 16, DOGE_COLOR);
        } else if (string_strcmp(boot_choice, "4") == 0) { 
            dogeio_clear_screen();
            dogeio_println("Such doge, very goodbye even without a such hi.");
            while(1) {
                __asm__ __volatile__("hlt");
            }
        }
    }
}
