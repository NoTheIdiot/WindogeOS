#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/string.h"
#include "../library/stddef.h"
#include "../library/stdint.h"
#include "../library/bool.h"

void help_me(void) {
    char* help[] = {
        "WindogeOS DogeShell Experimental",
        "\n exit (also is kys) print reboot shutdown ver help"
    };
    for (size_t i = 0; i < sizeof(help) / sizeof(help[0]); i++) {
        println(help[i]);
    }
}

int execute(char* argv) {
    if (str_strcmp(argv, "exit") == 0 || str_strcmp(argv, "kys") == 0) {
        exit(0);
    }

    if (str_strcmp(argv, "reboot") == 0) {
        reboot();
    }

    if (str_strcmp(argv, "shutdown") == 0) {
        shutdown();
    }
    
    if (str_startswith(argv, "print")) {
        println(argv + 6);
        return 0;
    }
    
    if (str_strcmp(argv, "clear") == 0) {
        clear();
        return 0;
    }

    if (str_strcmp(argv, "ver") == 0) {
        println("Experimental DogeShell");
        return 0;
    }

    if (str_strcmp(argv, "help") == 0) {
        help_me();
        return 0;
    }

    else {
        println("command not found");
        return -2;
    }
    return -127;
}

void _start(void) {
    // welcome
    println("Welcome to WindogeOS DogeShell experimental shell.");
    println("It's just so i can try and replace kernel space dogeshell, don't expect\
        \nit to be fully featured like the kernel space one (default).");

    // actually begin
    char command[256];
    char prompt[96];
    while (true) {
        str_strcpy(prompt, "experimental WindogeOS > ");

        input(prompt, command, 256);
        execute(command);
    }
    exit((uint64_t)-1);
}
