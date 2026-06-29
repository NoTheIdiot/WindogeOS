#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <system.h>

extern void hcf(void);

void system_dogeshell_execute(const char* command) {
    int handled = 0;

    if (command[0] == '\0') {
        return;
    }

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

    else if (string_strcmp(command, "shutdown") == 0 || string_strcmp(command, "goodbye") == 0) {
        dogeio_text_clear();
        dogeio_text_print("Such shutdown, very goodbye.");
        hcf();
    }

    else if (string_strcmp(command, "dir") == 0) {
        system_file_list_directory();
        handled = 1;
    }
    
    else if (string_strcmp(command, "cpuinfo") == 0) {
        char buffer[64];
        system_info_cpu(buffer);
        dogeio_text_println(buffer);
        handled = 1;
    } else if (string_strcmp(command, "raminfo") == 0) {
        char buffer[64];
        system_info_ram(buffer);
        dogeio_text_println(buffer);
        handled = 1;
    }

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
            dogeio_text_println("cpuinfo                | prints out the cpu info");
            dogeio_text_println("raminfo                | prints the ram info");
            dogeio_text_println("dir                    | lists directory/folder");
        } else if (string_strcmp(args, "print") == 0 || string_strcmp(args, "bark") == 0) {
            dogeio_text_println("usage: print/bark [message]");
            dogeio_text_println("prints some text, that's it.");
        } else if (string_strcmp(args, "goodbye") == 0 || string_strcmp(args, "shutdown") == 0) {
            dogeio_text_println("usage: goodbye/shutdown");
            dogeio_text_println("halts the system");
        } else if (string_strcmp(args, "clear") == 0) {
            dogeio_text_println("usage: clear");
            dogeio_text_println("clears the display");
        } else if (string_strcmp(args, "raminfo") == 0) {
            dogeio_text_println("usage: raminfo");
            dogeio_text_println("prints the ram information");
        } else if (string_strcmp(args, "cpuinfo") == 0) {
            dogeio_text_println("usage: cpuinfo");
            dogeio_text_println("prints out the cpu information");
        } else if (string_strcmp(args, "dir") == 0) {
            dogeio_text_println("usage: dir");
            dogeio_text_println("lists the root folder/directory");
        } else if (string_strcmp(args, "help") == 0) {
            dogeio_text_println("usage: help [command]");
            dogeio_text_println("shows usage rules for system utilities.");
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
