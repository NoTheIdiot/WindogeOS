#include <string.h>
#include <dogeio.h>
#include "../kernel/dogeshell.h"
#include <bool.h>
#include <consts.h>

void friendly_mode() {
    dogeio_clear_screen();
    const char *logo[] = {
        "__        ___           _                   ___  ____  ",
        "\\ \\      / (_)_ __   __| | ___   __ _  ___ / _ \\/ ___| ",
        " \\ \\ /\\ / /| | '_ \\ / _` |/ _ \\ / _` |/ _ \\ | | \\___ \\ ",
        "  \\ V  V / | | | | | (_| | (_) | (_| |  __/ |_| |___) |",
        "   \\_/\\_/  |_|_| |_|\\__,_|\\___/ \\__, |\\___|\\___/|____/ ",
        "                                |___/                  "
    };

    int lines = sizeof(logo) / sizeof(logo[0]);

    while (true) {
        char choice[32];
        dogeio_clear_screen();
        for (int i = 0; i < lines; i++) {

            if (logo[i] != NULL) {
                dogeio_println((char*)logo[i]);
            }
        }
        dogeio_println("Welcome to Friendly Mode!");

        dogeio_println("[1] Terminal");
        dogeio_println("[2] Calculator");
        dogeio_println("[3] Exit");

        dogeio_print("> ");
        dogeio_input(choice, 32, DOGE_COLOR);

        if (string_strcmp(choice, "1") == 0 || string_startswith(choice, "Terminal")) {
            dogeio_clear_screen();
            doge_shell(1);
            dogeio_clear_screen();
        } else if (string_strcmp(choice, "2") == 0 || string_startswith(choice, "Calculator")) {

        } else if (string_strcmp(choice, "3") == 0 || string_startswith(choice, "Exit")) {
            dogeio_clear_screen();
            return;
        } 
    }
}
