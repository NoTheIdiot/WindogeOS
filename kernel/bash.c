#include <stdint.h>
#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <time.h>
#include <system.h>

int system_bash_ex(char* command) {
    int handled = 1; 

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    if (string_strcmp(command, "echo") == 0 || string_startswith(command, "echo ")) {
        size_t len = string_strlen(command);
        if (len >= 5 && command[4] == ' ') {
            dogeio_text_println(command + 5);
        } else {
            dogeio_text_println(""); 
        }
        handled = 0;
    }

    else if (string_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 0;
    }

    else if (string_strcmp(command, "ls") == 0) {
        system_fs_list();
        handled = 0;
    }

    else if (string_startswith(command, "date")) {
        dogeio_text_println(time_get());
        handled = 0;
    }

    else if (string_startswith(command, "cat")) {
        size_t len = string_strlen(command);
        if (len >= 4 && command[3] == ' ') {
            char* argument = command + 4;
            char name[32] = {0};
            char ext[8] = {0};

            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: cat [filename.ext]");
                dogeio_text_println("Outputs the contents of a file.");
            } else {
                string_split_filename(argument, name, ext);
                if (name[0] != '\0') {
                    system_fs_readfile(name, ext);
                } else {
                    dogeio_text_println("Error: Invalid filename.");
                }
            }
        } else {
            dogeio_text_println("Usage: cat [filename.ext]");
        }
        handled = 0;
    }

    else if (string_strcmp(command, "fetch") == 0) {
        system_fetch();
        handled = 0;
    }

    else if (string_strcmp(command, "whoami") == 0) {
        dogeio_text_println("wow");
        handled = 0;
    }

    else if (string_strcmp(command, "pwd") == 0) {
        dogeio_text_println("root (/)");
        handled = 0;
    }

    else if (string_startswith(command, "sed")) {
        size_t len = string_strlen(command);
        if (len >= 4 && command[3] == ' ') {
            char* argument = command + 4;
            char name[32] = {0};
            char ext[8] = {0};

            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: sed [filename.ext]");
                dogeio_text_println("Writes a string of text to the file specified.");
            } else {
                string_split_filename(argument, name, ext);
                if (name[0] != '\0') {
                    char input[256];
                    dogeio_text_input("Text to Write:\n", input, 256);
                    system_fs_writefile(name, ext, input);
                } else {
                    dogeio_text_println("Error: Invalid filename.");
                }
            }
        } else {
            dogeio_text_println("Usage: sed [filename.ext]");
        }
        handled = 0;
    }

    else if (string_startswith(command, "touch")) {
        char name[32] = {0};
        char ext[8] = {0};
        dogeio_text_input("name > ", name, 32);
        dogeio_text_input("ext  > ", ext, 8);
        if (name[0] != '\0' && ext[0] != '\0') {
            system_fs_createfile(name, ext);
        } else {
            dogeio_text_println("Error: Name and extension cannot be empty.");
        }
        handled = 0;
    }

    else if (string_startswith(command, "rm ")) {
        size_t len = string_strlen(command);
        if (len >= 4) {
            char* argument = command + 3;
            char name[32] = {0};
            char ext[8] = {0};
            string_split_filename(argument, name, ext);
            if (name[0] != '\0') {
                system_fs_delete_file(name, ext);
            } else {
                dogeio_text_println("Error: Invalid filename.");
            }
        }
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

        if (string_strcmp(input, "exit") == 0) {
            return;
        }
        system_bash_ex(input);
    }
}
