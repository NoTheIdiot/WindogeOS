// include files cuz needed
#include <dogeio.h>
#include <string.h>
#include <bool.h>

// main
void system_dogeshell_execute(const char* command) {
    int handled = 0;

    if (command[0] == '\0') {
        return;
    }

    // BASIC
    if (string_startswith(command, "print ") || string_startswith(command, "bark ")) {
        int offset = string_startswith(command, "print ") ? 6 : 5;
        dogeio_text_println(command + offset);
        handled = 1;
    } else if (string_strcmp(command, "print") == 0 || string_strcmp(command, "bark") == 0) {
        dogeio_text_println("");
        handled = 1;
    } else if (string_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 1;
    }
    
    // INFORMATION
    else if (string_startswith(command, "help")) {
        const char *args = command + 4;
        if (*args == ' ') {
            args++;
        }

        if (string_strcmp(args, "") == 0) {
            dogeio_text_println("print/bark [text]      | prints text");
            dogeio_text_println("help [command]         | shows this help message");
            dogeio_text_println("shutdown/goodbye       | halts the system");
            dogeio_text_println("clear                  | clears the display");
        } else if (string_strcmp(args, "print") == 0 || string_strcmp(args, "bark") == 0) {
            dogeio_text_println("usage: print/bark [message]");
            dogeio_text_println("prints some text, that's it.");
        } else if (string_strcmp(args, "goodbye") == 0 || string_strcmp(args, "shutdown") == 0) {
            dogeio_text_println("usage: goodbye/shutdown");
            dogeio_text_println("halts the system");
        } else if (string_strcmp(args, "clear") == 0) {
            dogeio_text_println("usage: clear");
            dogeio_text_println("clears the display");
        } else {
            dogeio_text_println("command doesn't exist.");
        }
        handled = 1;
    }

    if (handled == 0) {
        dogeio_text_print(command);
        dogeio_text_println(": command not found");
    }
}

void system_dogeshell() {
    char command[128];
    while (true) {
        dogeio_text_input("wow (root) > ", command, 128);
        system_dogeshell_execute(command);
    }
}