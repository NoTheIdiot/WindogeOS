// include files
#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "../kernel/dogeshell.h"
#include "bool.h"

void friendly_mode() {
    // a logo for WindogeOS friendly mode
    const char *logo[] = {
        "__        ___           _                   ___  ____  ",
        "\\ \\      / (_)_ __   __| | ___   __ _  ___ / _ \\/ ___| ",
        " \\ \\ /\\ / /| | '_ \\ / _` |/ _ \\ / _` |/ _ \\ | | \\___ \\ ",
        "  \\ V  V / | | | | | (_| | (_) | (_| |  __/ |_| |___) |",
        "   \\_/\\_/  |_|_| |_|\\__,_|\\___/ \\__, |\\___|\\___/|____/ ",
        "                                |___/                  "
    };

    // calculate the amount of lines inside of array logo
    int lines = sizeof(logo) / sizeof(logo[0]);

    // loop friendly mode
    while (true) {
        char choice[32];
        // print the logo
        for (int i = 0; i < lines; i++) {
            dogeio_println(logo[i]);
        }

        dogeio_println("[1]");
    }
}