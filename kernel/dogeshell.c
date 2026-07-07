#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <system.h>
#include <time.h>
#include <math.h>

extern void hcf(void);
extern uint32_t dogeio_text_tcolor;
extern void draw_menu_bar(void);

void shell_print_float(float num) {
    char buf[32];
    int ipart = (int)num;
    float fpart = num - (float)ipart;
    
    if (fpart < 0.0f) {
        fpart = -fpart;
        if (ipart == 0) {
            dogeio_text_print("-");
        }
    }
    
    string_itoa(ipart, buf);
    dogeio_text_print(buf);
    dogeio_text_print(".");
    
    int fpart_int = (int)(fpart * 10000.0f);
    if (fpart_int < 0) fpart_int = -fpart_int;
    
    if (fpart_int < 1000) dogeio_text_print("0");
    if (fpart_int < 100)  dogeio_text_print("0");
    if (fpart_int < 10)   dogeio_text_print("0");
    
    string_itoa(fpart_int, buf);
    dogeio_text_print(buf);
}

void parse_two_floats(const char* args, float* f1, float* f2) {
    *f1 = string_atof(args);
    while (*args && *args != ' ') {
        args++;
    }
    while (*args == ' ') {
        args++;
    }
    *f2 = string_atof(args);
}

// many functions better than big function
static void math_subcommand(const char *subcommand) {
    if (string_startswith(subcommand, "add ")) {
        float f1, f2;
        parse_two_floats(subcommand + 4, &f1, &f2);
        shell_print_float(f1 + f2);
        dogeio_text_println("");
    } else if (string_startswith(subcommand, "sub ")) {
        float f1, f2;
        parse_two_floats(subcommand + 4, &f1, &f2);
        shell_print_float(f1 - f2);
        dogeio_text_println("");
    } else if (string_startswith(subcommand, "mul ")) {
        float f1, f2;
        parse_two_floats(subcommand + 4, &f1, &f2);
        shell_print_float(f1 * f2);
        dogeio_text_println("");
    } else if (string_startswith(subcommand, "pow ")) {
        float f1, f2;
        parse_two_floats(subcommand + 4, &f1, &f2);
        shell_print_float(math_power(f1, f2));
        dogeio_text_println("");
    } else if (string_startswith(subcommand, "root ")) {
        float f1, f2;
        parse_two_floats(subcommand + 5, &f1, &f2);
        if (f1 < 0.0f) {
            dogeio_text_println("Error: Base cannot be negative.");
        } else {
            shell_print_float(math_root(f1, f2));
            dogeio_text_println("");
        }
    } else {
        dogeio_text_println("usage: math [add|sub|mul|pow|root] [a] [b]");
    }
}

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

    else if (string_startswith(command, "wait")) {
        const char* arg = command + 4;

        while (*arg == ' ' || *arg == '\t') {
            arg++;
        }

        if (*arg == '\0') {
            dogeio_text_print("Error: Please specify the number of seconds.\n");
        } else {
            int waiting = string_atoi((char*)arg);
            time_wait_sec(waiting);
        }
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
	} else if (string_startswith(command, "readfile")) {
		//system_file_readfile(command + 9);
		handled = 1;
	} else if (string_startswith(command, "writefile")) {
		char* arg = command + 10;
		char input_buffer[256];
		char short_name[8];

		for (int i = 0; i < 256; i++) {
			input_buffer[i] = '\0';
		}
		for (int i = 0; i < 8; i++) {
			short_name[i] = '\0';
		}

		int arg_idx = 0;
		while (arg[arg_idx] != '\0' && arg[arg_idx] != '.' && arg_idx < 8) {
			short_name[arg_idx] = arg[arg_idx];
			arg_idx++;
		}

		dogeio_text_input("write text> ", input_buffer, 256);

		uint32_t len = 0;
		while (input_buffer[len] != '\0') {
			len++;
		}

		system_file_create(short_name, "TXT", input_buffer, len);
		dogeio_text_println("FAT32 cluster block operation completed.");
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

    else if (string_strcmp(command, "ver") == 0) {
        dogeio_text_println("WindogeOS V0.0.2");
        handled = 1;
    }

    else if (string_strcmp(command, "math") == 0) {
        dogeio_text_println("usage: math [add|sub|mul|pow|root] [a] [b]");
        handled = 1;
    }

    else if (string_startswith(command, "math ")) {
        math_subcommand(command + 5);
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
            dogeio_text_println("math [add|sub|mul|pow|root] [a] [b] | math operations");
            dogeio_text_println("root [base] [n]        | calculates n-th root");
            dogeio_text_println("ver                    | shows version.");
            dogeio_text_println("wait [seconds]         | waits");
            dogeio_text_println("readfile [filename]    | reads and outputs an entire file.");
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
        } else if (string_strcmp(args, "add") == 0 || string_strcmp(args, "sub") == 0 || string_strcmp(args, "mul") == 0 || string_strcmp(args, "pow") == 0) {
            dogeio_text_println("usage: command [num1] [num2]");
            dogeio_text_println("performs standard mathematical operations.");
        } else if (string_strcmp(args, "root") == 0) {
            dogeio_text_println("usage: root [base] [n-th_root]");
            dogeio_text_println("calculates the n-th root using an estimation method.");
            dogeio_text_println("may not be accurate.");
        } else if (string_strcmp(args, "wait") == 0) {
            dogeio_text_println("usage: wait [seconds]");
            dogeio_text_println("waits in seconds.");
        } else if (string_strcmp(args, "readfile") == 0) {
            dogeio_text_println("usage: readfile [filename]");
            dogeio_text_println("outputs an entire file from the disk.");
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
        draw_menu_bar();
        dogeio_text_input("wow (root) > ", command, 128);
        system_dogeshell_execute(command);
    }
}