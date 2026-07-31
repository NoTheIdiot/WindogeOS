/*
This is a shell for development, if everything works, integrated it into dogeshell.
why am i doing this
*/

#include <string.h>
#include <bool.h>
#include <dogeio.h>

char* shit[7] = {
    "h                  - help",
    "f                  - format",
    "c                  - create file",
    "d                  - delete file",
    "l                  - list files",
    "r                  - read file",
    "cs                 - clear"
};

void fstest_shell_ex(char* command) {

    if (command[0] == '\0') {
        return;
    }

    if (str_strcmp(command,"h") == 0) {
        for (int i = 0; i< 7; i++) {
            dogeio_text_println(shit[i]);
        }
    }

    else if (str_strcmp(command,"f") == 0) {
        fs_format();
    }

    else if (str_strcmp(command, "l")==0) {
        fs_list_directory();
    }

    else if (str_strcmp(command,"cs")==0) {
        dogeio_text_clear();
    }

    else if (str_strcmp(command,"c")==0) {
        char input_name[32];
        dogeio_text_input("name: ", input_name, 32);
        fs_create(input_name);
    }

    else {
        dogeio_text_println("oops");
    }
}

void fstest_shell() {
    char input[4];
    dogeio_text_println("Welcome to WindogeOS tester lol.");
    for (int i = 0; i< 7; i++) {
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