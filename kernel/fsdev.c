#include <string.h>
#include <bool.h>
#include <dogeio.h>

extern void duolog(const char* message);

char* shit[8] = {
    "h                  - help",
    "f                  - format",
    "c                  - create file",
    "d                  - delete file",
    "l                  - list files",
    "w                  - write file",
    "r                  - read file",
    "cs                 - clear"
};

void fstest_shell_ex(char* command) {
    if (command[0] == '\0') {
        return;
    }

    if (str_strcmp(command, "h") == 0) {
        for (int i = 0; i < 8; i++) {
            dogeio_text_println(shit[i]);
        }
    }
    else if (str_strcmp(command, "f") == 0) {
        fs_format();
    }
    else if (str_strcmp(command, "l") == 0) {
        fs_list_dir();
    }
    else if (str_strcmp(command, "cs") == 0) {
        dogeio_text_clear();
    }
    else if (str_strcmp(command, "c") == 0) {
        char input_name[32];
        dogeio_text_input("name: ", input_name, 32);
        fs_create(input_name);
    }
    else if (str_strcmp(command, "d") == 0) {
        char input_name[32];
        dogeio_text_input("name: ", input_name, 32);
        fs_delete(input_name);
    }
    else if (str_strcmp(command, "w") == 0) {
        char input_name[32];
        static char text[256];
        
        dogeio_text_input("name: ", input_name, 32);
        dogeio_text_input("text: ", text, 256);
        
        int result = fs_write(input_name, text);
        if (result == 1) {
            dogeio_text_println("write ok");
        } else if (result == 0) {
            dogeio_text_println("Error: disk full.");
        } else if (result == -2) {
            dogeio_text_println("Error: file not found.");
        } else {
            dogeio_text_println("Error: write failed.");
        }
    }

    else if (str_strcmp(command, "r") == 0) {
        char input_name[32];
        static char output_buffer[8192];
        dogeio_text_input("name: ", input_name, 32);

        int bytes_read = fs_read(input_name, output_buffer, sizeof(output_buffer));
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
    }

    else {
        dogeio_text_println("oops");
    }
}

void fstest_shell() {
    char input[4];
    dogeio_text_println("Welcome to WindogeOS tester lol.");
    for (int i = 0; i < 7; i++) {
        dogeio_text_println(shit[i]);
    }
    while (true) {
        dogeio_text_input(": ", input, 4);

        if (str_strcmp(input, "e") == 0) {
            return;
        }
        fstest_shell_ex(input);
    }
}
