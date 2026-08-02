#include <stdint.h>
#include <dogeio.h>
#include <basicutil.h>
#include <string.h>
#include <bool.h>
#include <time.h>
#include <system.h>

char history[32][64];

int system_bash_ex(char* command) {
    int handled = 1; 

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    if (str_strcmp(command, "echo") == 0 || str_startswith(command, "echo ")) {
        size_t len = str_strlen(command);
        if (len >= 5 && command[4] == ' ') {
            dogeio_text_println(command + 5);
        } else {
            dogeio_text_println(""); 
        }
        handled = 0;
    }

    else if (str_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 0;
    }

    else if (str_strcmp(command, "ls") == 0) {
        fs_list_dir();
        handled = 0;
    }

    else if (str_startswith(command, "date")) {
        dogeio_text_println(time_get());
        handled = 0;
    }

	else if (str_startswith(command, "color")) {
        const char *arg = command + 6;
        if (str_strcmp(arg, "black") == 0) {
            dogeio_text_color = 0xFF000000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "white") == 0) {
            dogeio_text_color = 0xFFFFFFFF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "grey") == 0) {
            dogeio_text_color = 0xFF808080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "dark_grey") == 0) {
            dogeio_text_color = 0xFF404040;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "red") == 0) {
            dogeio_text_color = 0xFFFF0000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "green") == 0) {
            dogeio_text_color = 0xFF00FF00;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "blue") == 0) {
            dogeio_text_color = 0xFF0000FF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "yellow") == 0) {
            dogeio_text_color = 0xFFFFFF00;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "cyan") == 0) {
            dogeio_text_color = 0xFF00FFFF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "magenta") == 0) {
            dogeio_text_color = 0xFFFF00FF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "navy") == 0) {
            dogeio_text_color = 0xFF000080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "maroon") == 0) {
            dogeio_text_color = 0xFF800000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "teal") == 0) {
            dogeio_text_color = 0xFF008080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "olive") == 0) {
            dogeio_text_color = 0xFF808000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "doge_gold") == 0) {
            dogeio_text_color = 0xFFE1B857;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "doge_tan") == 0) {
            dogeio_text_color = 0xFFF4DFB1;
            dogeio_text_clear();
        } else {
            dogeio_text_println("unknown colorpreset.");
        }
        handled = 1;
    }

    else if (str_startswith(command, "cat")) {
        char* filename = command + 4;
        static char output_buffer[8192];

        int bytes_read = fs_read(filename, output_buffer, sizeof(output_buffer));
        if (bytes_read < 0) {
            dogeio_text_println("Error: unable to read file.");
        } else {
            char *line = output_buffer;
            size_t processed = 0;
            while ((int)processed < bytes_read) {
                dogeio_text_println(line);
                size_t line_len = str_strlen(line);
                processed += line_len + 1;
                line += line_len + 1;
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "fetch") == 0) {
        system_fetch();
        handled = 0;
    }

    else if (str_strcmp(command, "whoami") == 0) {
        dogeio_text_println("wow");
        handled = 0;
    }

    else if (str_strcmp(command, "pwd") == 0) {
        dogeio_text_println("root (/)");
        handled = 0;
    }

    else if (str_startswith(command, "sed")) {
        char* filename = command + 4;
        static char text[256];
        
        dogeio_text_input("text> ", text, 256);
        
        int result = fs_write(filename, text);
        if (result == 1) {
            dogeio_text_println("write ok");
        } else if (result == 0) {
            dogeio_text_println("Error: disk full.");
        } else if (result == -2) {
            dogeio_text_println("Error: file not found.");
        } else {
            dogeio_text_println("Not Wow: Something went wrong.");
        }
        handled = 0;
    }

    else if (str_startswith(command, "touch")) {
        char* filename = command + 6;
		int result = fs_create(filename);

        if (result) {
            dogeio_text_println("Not Wow: Failed to create file.");
            dogeio_text_println("File name potentially invalid: nothing cant be a filename.");
        }
        handled = 0;
    }

    else if (str_startswith(command, "rm ")) {
        char* filename = command + 3;

        int result = fs_delete(filename);
        if (!result) {
            dogeio_text_println("Not Wow: File Not Found.");
        } else if (result == -1) {
            dogeio_text_println("Doge Sad: Something went wrong!");
        } else if (result == -2) {
            dogeio_text_println("Error: File does not exist.");
        }
        handled = 0;
    }

	else if (str_strcmp(command, "shutdown") == 0) {
		dogeio_text_clear_raw();
		dogeio_text_println("Such shutdown, very goodbye.");
		halt();
		handled = 0;
	}

    if (handled == 1) {
        dogeio_text_print(command);
        dogeio_text_println(": command not found");
    }

    return handled;
}

void system_bash() {
    char input[256];

    while (true) {
        for (int i = 0; i < 256; i++) {
            input[i] = '\0';
        }

        dogeio_text_color_change(0x0000FF00);
        dogeio_text_print("wow");
        dogeio_text_color_change(0x000000FF);
        dogeio_text_print(":/");

        dogeio_text_color_change(0xFFFFFFFF);
        dogeio_text_input("$ ", input, 256);

        if (str_strcmp(input, "exit") == 0) {
            return;
        }
        system_bash_ex(input);
    }
}
