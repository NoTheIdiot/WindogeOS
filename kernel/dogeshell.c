#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <system.h>
#include <time.h>

extern void hcf(void);
extern uint32_t dogeio_text_tcolor;

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

    else if (string_startswith(command, "color")) {
        const char *arg = command + 6;
        if (string_strcmp(arg, "black") == 0) {
            dogeio_text_tcolor = 0xFF000000;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "white") == 0) {
            dogeio_text_tcolor = 0xFFFFFFFF;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "grey") == 0) {
            dogeio_text_tcolor = 0xFF808080;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "dark_grey") == 0) {
            dogeio_text_tcolor = 0xFF404040;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "red") == 0) {
            dogeio_text_tcolor = 0xFFFF0000;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "green") == 0) {
            dogeio_text_tcolor = 0xFF00FF00;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "blue") == 0) {
            dogeio_text_tcolor = 0xFF0000FF;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "yellow") == 0) {
            dogeio_text_tcolor = 0xFFFFFF00;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "cyan") == 0) {
            dogeio_text_tcolor = 0xFF00FFFF;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "magenta") == 0) {
            dogeio_text_tcolor = 0xFFFF00FF;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "navy") == 0) {
            dogeio_text_tcolor = 0xFF000080;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "maroon") == 0) {
            dogeio_text_tcolor = 0xFF800000;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "teal") == 0) {
            dogeio_text_tcolor = 0xFF008080;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "olive") == 0) {
            dogeio_text_tcolor = 0xFF808000;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "doge_gold") == 0) {
            dogeio_text_tcolor = 0xFFE1B857;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "doge_tan") == 0) {
            dogeio_text_tcolor = 0xFFF4DFB1;
            dogeio_text_clear();
        } else {
            dogeio_text_println("unknown colorpreset.");
        }
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

    else if (string_strcmp(command, "time") == 0) {
        dogeio_text_print("such current time: ");
        time_show();
        dogeio_text_println("");
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
            dogeio_text_println("color [preset]         | clears screen and changes font color");
            dogeio_text_println("cpuinfo                | prints out the cpu info");
            dogeio_text_println("raminfo                | prints the ram info");
            dogeio_text_println("dir                    | lists directory/folder");
            dogeio_text_println("time                   | shows the current time");
            dogeio_text_println("color [color]          | change color of text.");
        } else if (string_strcmp(args, "print") == 0 || string_strcmp(args, "bark") == 0) {
            dogeio_text_println("usage: print/bark [message]");
            dogeio_text_println("prints some text, that's it.");
        } else if (string_strcmp(args, "goodbye") == 0 || string_strcmp(args, "shutdown") == 0) {
            dogeio_text_println("usage: goodbye/shutdown");
            dogeio_text_println("halts the system");
        } else if (string_strcmp(args, "clear") == 0) {
            dogeio_text_println("usage: clear");
            dogeio_text_println("clears the display");
        } else if (string_strcmp(args, "color") == 0) {
            dogeio_text_println("usage: color [color]");
            dogeio_text_println("colors: black, white, grey, dark_grey, red, green, blue,");
            dogeio_text_println("         yellow, cyan, magenta, navy, maroon, teal, olive,");
            dogeio_text_println("         doge_gold, doge_tan");
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
        } else if (string_strcmp(args, "time") == 0) {
            dogeio_text_println("usage: time");
            dogeio_text_println("outputs the time using RTC, remember to");
            dogeio_text_println("replace your cmos battery or it will desync.");
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
