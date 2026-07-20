#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <system.h>

int system_dogeshell_ex(char* command) {
    int handled = 1; 
    
    char* help_command_array[6] = {
        "print          | simply prints a piece of text",
        "clear          | clears the terminal screen",
        "dir            | list the contents of the current folder",
        "readfile       | outputs the contents of a file",
        "help           | outputs this help menu breakdown",
        "shutdown       | shutsdown the computer"
    };

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    if (string_startswith(command, "print")) {
        size_t len = string_strlen(command);
        
        if (len >= 6 && command[5] == ' ') {
            dogeio_text_println(command + 6);
        } else {
            dogeio_text_println(""); 
        }
        handled = 0;
    }

    else if (string_startswith(command, "clear")) {
        dogeio_text_clear();
        handled = 0;
    }

    else if (string_startswith(command, "dir")) {
        system_fs_list();
        handled = 0;
    }

    else if (string_startswith(command, "readfile")) {
        size_t len = string_strlen(command);
        
        if (len >= 10 && command[8] == ' ') {
            char* argument = command + 9;

            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: readfile [filename]");
                dogeio_text_println("Outputs the contents of a file.");
            } else {
				system_fs_readfile(argument);
            }
        } else {
            dogeio_text_println("Usage: readfile [filename]");
        }
        handled = 0;
    }

    else if (string_startswith(command, "help")) {
        for (int i = 0; i < 6; i++) {
            dogeio_text_println(help_command_array[i]);
        }
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
    char prompt[16] = "wow (root) ";
    int  status     = 0;

    while (true) {
        dogeio_text_print(prompt);
        if (status == 1) {
            dogeio_text_print("[1] ");
        }

        for (int i = 0; i < 256; i++) {
            input[i] = '\0';
        }

        dogeio_text_input("> ", input, 256);
        status = system_dogeshell_ex(input);
    }
}
