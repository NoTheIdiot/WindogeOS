#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <system.h>

int system_dogeshell_ex(char* command) {
    int handled = 1; 
    
    char* help_command_array[7] = {
        "print          | simply prints a piece of text",
        "clear          | clears the terminal screen",
        "dir            | list the contents of the current folder",
        "readfile       | outputs the contents of a file",
        "writefile      | writes a string of text to a file",
        "help           | outputs this help menu breakdown",
        "shutdown       | shutsdown the computer"
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

    else if (string_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 0;
    }

    else if (string_strcmp(command, "dir") == 0) {
        system_fs_list();
        handled = 0;
    }

    else if (string_startswith(command, "readfile")) {
        size_t len = string_strlen(command);
        if (len >= 9 && command[8] == ' ') {
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

    else if (string_startswith(command, "writefile")) {
        size_t len = string_strlen(command);
        if (len >= 10 && command[9] == ' ') {
            char* argument = command + 10;
            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: writefile [filename]");
                dogeio_text_println("Writes a string of text to the file specified.");
            } else if (1) {

			} else {
                char input[256];
                dogeio_text_input("Text to Write:\n", input, 256);
                system_fs_writefile(argument, input);
            }
        } else {
            dogeio_text_println("Usage: writefile [filename]");
        }
        handled = 0;
    }

    else if (string_strcmp(command, "help") == 0) {
        for (int i = 0; i < 7; i++) {
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
    char prompt[32];
    int  status     = 0;

    while (true) {
        for (int i = 0; i < 256; i++) {
            input[i] = '\0';
        }

        if (status == 1) {
            string_strcpy(prompt, "wow (root) [1] > ");
        } else {
            string_strcpy(prompt, "wow (root) > ");
        }

        dogeio_text_input(prompt, input, 256);
        status = system_dogeshell_ex(input);
    }
}
