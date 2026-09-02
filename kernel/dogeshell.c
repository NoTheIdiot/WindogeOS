#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <system.h>
#include <core.h>
#include <basicutil.h>
#include <image.h>
#include <bool.h>

uint32_t saved_color = 0xFFCCCCCC;

int system_dogeshell_ex(char* command) {
    int handled = 1;

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    if (str_strcmp(command, "print") == 0 || str_startswith(command, "print ")) {
        if (str_startswith(command, "print ")) {
            dogeio_text_println(command + 6);
        } else {
            dogeio_text_println(""); 
        }
        handled = 0;
    }

    if (str_strcmp(command, "help") == 0) {
        char buffer[2048];
        fs_read(".help")
    }
}