#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <basicutil.h>
#include <system.h>
#include <time.h>

extern uint64_t get_ram(void);

int system_dogeshell_ex(char* command) {
    int handled = 1; 
    
    char* help_command_array[8] = {
        "print          | simply prints a piece of text",
        "clear          | clears the terminal screen",
        "dir            | list the contents of the current folder",
        "readfile       | outputs the contents of a file",
        "writefile      | writes a string of text to a file",
        "help           | outputs this help menu breakdown",
        "shutdown       | shutsdown the computer",
        "cpuinfo        | prints the cpu name"
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

    else if (string_startswith(command, "time")) {
        dogeio_text_println(time_get());
        handled = 0;
    }

    else if (string_startswith(command, "readfile")) {
        size_t len = string_strlen(command);
        if (len >= 9 && command[8] == ' ') {
            char* argument = command + 9;
            char name[32] = {0};
            char ext[8] = {0};

            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: readfile [filename.ext]");
                dogeio_text_println("Outputs the contents of a file.");
            } else {
                string_split_filename(argument, name, ext);
                if (name[0] != '\0') {
                    system_fs_readfile(name, ext);
                } else {
                    dogeio_text_println("Error: Invalid filename specified.");
                }
            }
        } else {
            dogeio_text_println("Usage: readfile [filename.ext]");
        }
        handled = 0;
    }

    else if (string_startswith(command, "writefile")) {
        size_t len = string_strlen(command);
        if (len >= 10 && command[9] == ' ') {
            char* argument = command + 10;
            char name[32] = {0};
            char ext[8] = {0};

            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: writefile [filename.ext]");
                dogeio_text_println("Writes a string of text to the file specified.");
            } else {
                string_split_filename(argument, name, ext);
                if (name[0] != '\0') {
                    char input[256];
                    dogeio_text_input("Text to Write:\n", input, 256);
                    system_fs_writefile(name, ext, input);
                } else {
                    dogeio_text_println("Error: Invalid filename specified.");
                }
            }
        } else {
            dogeio_text_println("Usage: writefile [filename.ext]");
        }
        handled = 0;
    }

    else if (string_startswith(command, "createfile")) {
        char name[32] = {0};
        char ext[8] = {0};
        dogeio_text_input("name > ", name, 32);
        dogeio_text_input("ext  > ", ext, 8);
        if (name[0] != '\0' && ext[0] != '\0') {
            system_fs_createfile(name, ext);
        } else {
            dogeio_text_println("Error: File name and extension cannot be empty.");
        }
        handled = 0;
    }

    else if (string_startswith(command, "deletefile")) {
        size_t len = string_strlen(command);
        if (len >= 11 && command[10] == ' ') {
            char* argument = command + 11;
            char name[32] = {0};
            char ext[8] = {0};

            if (string_strcmp(argument, "--help") == 0) {
                dogeio_text_println("Usage: deletefile [filename.ext]");
                dogeio_text_println("Deletes the specified file permanently from RAM.");
            } else {
                string_split_filename(argument, name, ext);
                if (name[0] != '\0') {
                    system_fs_delete_file(name, ext);
                } else {
                    dogeio_text_println("Error: Invalid filename specified.");
                }
            }
        } else {
            dogeio_text_println("Usage: deletefile [filename.ext]");
        }
        handled = 0;
    }

    else if (string_strcmp(command, "help") == 0) {
        for (int i = 0; i < 8; i++) {
            dogeio_text_println(help_command_array[i]);
        }
        handled = 0;
    }

    else if (string_startswith(command, "cpuinfo")) {
        dogeio_text_print("Cpu Name: ");
        dogeio_text_println(cpuid());
        handled = 0;
    }

    else if (string_strcmp(command, "fetch") == 0) {
        system_fetch();
        handled = 0;
    }

    else if (string_startswith(command, "raminfo")) {
        dogeio_text_print("RAM Amount: ");
        char ram_buf[16];
        string_itoa((int)(get_ram() / 1024 / 1024), ram_buf);
        dogeio_text_print(ram_buf);
        dogeio_text_println(" MB");
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
            dogeio_text_color_change(0xFFFFFFFF);
            dogeio_text_print(" (root) ");
            dogeio_text_color_change(0x00FF0000);
            dogeio_text_print("[1] ");
        } else {
            dogeio_text_color_change(0x0000FF00);
            dogeio_text_print("wow");
            dogeio_text_color_change(0xFFFFFFFF);
            dogeio_text_print(" (root) ");
        }

        dogeio_text_color_change(0xFFFFFFFF);
        dogeio_text_input("> ", input, 256);
        status = system_dogeshell_ex(input);
    }
}
