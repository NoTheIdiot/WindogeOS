#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <basicutil.h>
#include <system.h>
#include <time.h>

uint32_t old = 0xffffff;
extern void fstest_shell();

int system_dogeshell_ex(char* command) {
    int handled = 1; 
    
    char* help_command_array[16] = {
        "print              | simply prints a piece of text",
        "clear              | clears the terminal screen",
        "dir                | list the contents of the current folder",
        "readfile           | outputs the contents of a file",
        "writefile          | writes a string of text to a file",
        "help               | outputs this help menu breakdown",
        "shutdown           | shutsdown the computer",
        "cpuinfo            | prints the cpu name",
		"deletefile	        | deletes a file",
		"createfile         | creates a file",
		"whoami	            | displays your current username",
		"whereami           | displays your current folder location",
		"time               | displays the time",
		"ver                | shows version",
		"fetch              | shows the system information",
		"color              | changes color of text."
    };

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    if (string_strcmp(command, "print") == 0 || string_startswith(command, "print ")) {
        size_t len = string_strlen(command);
        if (len >= 6 && command[5] == ' ') {
            dogeio_text_println(command + 6);
        } else {
            dogeio_text_println(""); 
        }
        handled = 0;
    }

	else if (string_startswith(command, "color")) {
        const char *arg = command + 6;
        if (string_strcmp(arg, "black") == 0) {
            dogeio_text_color = 0xFF000000;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "white") == 0) {
            dogeio_text_color = 0xFFFFFFFF;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "grey") == 0) {
            dogeio_text_color = 0xFF808080;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "dark_grey") == 0) {
            dogeio_text_color = 0xFF404040;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "red") == 0) {
            dogeio_text_color = 0xFFFF0000;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "green") == 0) {
            dogeio_text_color = 0xFF00FF00;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "blue") == 0) {
            dogeio_text_color = 0xFF0000FF;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "yellow") == 0) {
            dogeio_text_color = 0xFFFFFF00;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "cyan") == 0) {
            dogeio_text_color = 0xFF00FFFF;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "magenta") == 0) {
            dogeio_text_color = 0xFFFF00FF;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "navy") == 0) {
            dogeio_text_color = 0xFF000080;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "maroon") == 0) {
            dogeio_text_color = 0xFF800000;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "teal") == 0) {
            dogeio_text_color = 0xFF008080;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "olive") == 0) {
            dogeio_text_color = 0xFF808000;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "doge_gold") == 0) {
            dogeio_text_color = 0xFFE1B857;
            dogeio_text_clear();
        } else if (string_strcmp(arg, "doge_tan") == 0) {
            dogeio_text_color = 0xFFF4DFB1;
            dogeio_text_clear();
        } else {
            dogeio_text_println("unknown colorpreset.");
        }
		old = dogeio_text_color;
        handled = 0;
    }

	else if (string_strcmp(command, "shutdown") == 0) {
		dogeio_text_clear_raw();
		dogeio_text_println("Such shutdown, very goodbye.");
		halt();
		handled = 0;
	}

    else if (string_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 0;
    }

    else if (string_strcmp(command, "dir") == 0) {
        system_fs_list();
        handled = 0;
    }

    // alright it actually works
    else if (string_startswith(command, "time")) {
        dogeio_text_println(time_get());
        handled = 0;
    }

    else if (string_startswith(command, "readfile")) {
        size_t len = string_strlen(command);
        if (len >= 9 && command[8] == ' ') {
            char* argument = command + 9;
	        char name[32];
	      	char ext[8];

			string_split_filename(argument, name, ext);
            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: readfile [filename]");
                dogeio_text_println("Outputs the contents of a file.");
            } else {
                system_fs_readfile(name, ext);
            }
        } else {
            dogeio_text_println("Usage: readfile [filename]");
        }
        handled = 0;
    }

    else if (string_startswith(command, "writefile")) {
        size_t len = string_strlen(command);
        if (len >= 10 && command[9] == ' ') {
            char* argument = command + 10;
			char name[32];
			char ext[8];

			string_split_filename(argument, name, ext);

            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: writefile [filename]");
                dogeio_text_println("Writes a string of text to the file specified.");
            } else if (string_strcmp(argument, "--delete") == 0) {

			} else {
                char input[256];
                dogeio_text_input("Text to Write:\n", input, 256);
                system_fs_writefile(name, ext, input);
            }
        } else {
            dogeio_text_println("Usage: writefile [filename]");
        }
        handled = 0;
    }

	else if (string_startswith(command, "createfile")) {
		char name[32];
		char ext[8];
		dogeio_text_input("name > ", name, 32);
		dogeio_text_input("ext  > ", ext, 8);
		system_fs_createfile(name, ext);
		handled = 0;
	}

	else if (string_startswith(command, "deletefile")) {
		size_t len = string_strlen(command);
        if (len >= 10 && command[9] == ' ') {
            char* argument = command + 10;
			char name[32];
			char ext[8];

			string_split_filename(argument, name, ext);

            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: writefile [filename]");
                dogeio_text_println("Writes a string of text to the file specified.");
            } else if (string_strcmp(argument, "--delete") == 0) {

			} else {
				system_fs_delete_file(name, ext);
            }
        } else {
            dogeio_text_println("Usage: writefile [filename]");
        }
        handled = 0;
	}

    else if (string_strcmp(command, "help") == 0) {
        for (int i = 0; i < 16; i++) {
            dogeio_text_println(help_command_array[i]);
        }
        handled = 0;
    }

	else if (string_startswith(command, "cpuinfo")) {
		dogeio_text_println("Cpu Name: ");
		dogeio_text_println(cpuid());
		handled = 0;
	}

	else if (string_strcmp(command, "fetch") == 0) {
		system_fetch();
		handled = 0;
	}

	else if (string_startswith(command, "raminfo")) {
		dogeio_text_print("RAM Amount: ");
		handled = 0;
	}

	else if (string_strcmp(command, "bash") == 0) {
		system_bash();
		handled = 0;
	}

	else if (string_strcmp(command, "ver") == 0) {
		dogeio_text_println(windoge_version);
		handled = 0;
	}

	else if (string_strcmp(command, "whoami") == 0) {
		dogeio_text_println("wow");
		handled = 0;
	}
	
	else if (string_strcmp(command, "whereami") == 0) {
		dogeio_text_println("root (/)");
		handled = 0;
	}

    else if (string_strcmp(command, "test") == 0) {
        fstest_shell();
        handled = 0;
    }

    if (handled == 1) {
        dogeio_text_print(command);
        dogeio_text_println(": command doesn't exist :(");
    }

    return handled;
}

void system_dogeshell() {
    char input[256];
    int  status = 0;

    while (true) {
        for (int i = 0; i < 256; i++) {
            input[i] = '\0';
        }

        if (status == 1) {
			dogeio_text_color_change(0x0000FF00);
            dogeio_text_print("wow");
			dogeio_text_color_change(old);
			dogeio_text_print(" (root) ");
			dogeio_text_color_change(0x00FF0000);
			dogeio_text_print("[1] ");
        } else {
            dogeio_text_color_change(0x0000FF00);
            dogeio_text_print("wow");
			dogeio_text_color_change(old);
			dogeio_text_print(" (root) ");
        }
		
		dogeio_text_color_change(old);
        dogeio_text_input("> ", input, 256);
        status = system_dogeshell_ex(input);
    }
}
