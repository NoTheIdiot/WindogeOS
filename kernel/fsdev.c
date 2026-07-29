/*
This is a shell for development, if everything works, integrated it into dogeshell.
why am i doing this
*/

#include <string.h>
#include <bool.h>
#include <dogeio.h>

char* shit[6] = {
    "h                  - help",
    "f                  - format",
    "c                  - create file",
    "d                  - delete file",
    "r                  - read file",
    "cs                  - clear"
};

void fstest_shell_ex(char* command) {

    if (command[0] == '\0') {
        return;
    }

    if (string_strcmp(command,"h") == 0) {
        for (int i = 0; i< 6; i++) {
            dogeio_text_println(shit[i]);
        }
    }

    else if (string_strcmp(command,"f") == 0) {
        fs_format();
    }

    else if (string_strcmp(command,"cs")==0) {
        dogeio_text_clear();
        for (int i = 0; i< 6; i++) {
            dogeio_text_println(shit[i]);
        }
    }

    else {
        dogeio_text_println("oops");
    }
}

void fstest_shell() {
    char input[4];
    dogeio_text_println("Welcome to WindogeOS tester lol.");
    for (int i = 0; i< 6; i++) {
        dogeio_text_println(shit[i]);
    }
    while (true) {
        dogeio_text_input(": ", input, 4);

        if (string_strcmp(input, "e") == 0) {
            return;
        }
        fstest_shell_ex(input);
    }
}