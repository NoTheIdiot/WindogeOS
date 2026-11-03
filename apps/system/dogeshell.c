#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/string.h"
#include "../library/stddef.h"
#include "../library/stdint.h"
#include "../library/bool.h"

#define MAX_ARGS 16
#define ARG_SIZE 64

void help_me(void) {
    char* help[] = {
        "WindogeOS DogeShell Experimental",
        "\n exit (also is kys) print reboot shutdown ver help"
    };
    for (size_t i = 0; i < sizeof(help) / sizeof(help[0]); i++) {
        println(help[i]);
    }
}

int execute(int argc, char** argv) {
    if (argc == 0) {
        return 0;
    }
    
    if (str_strcmp(argv[0], "exit") == 0 || str_strcmp(argv[0], "kys") == 0) {
        exit(0);
        return 0;
    }

    if (str_strcmp(argv[0], "reboot") == 0) {
        reboot();
        return 0;
    }

    if (str_strcmp(argv[0], "shutdown") == 0) {
        shutdown();
        return 0;
    }
    
    if (str_strcmp(argv[0], "print") == 0) {
        if (argc > 1) {
            println(argv[1]); 
        } else {
            println("");
        }
        return 0;
    }
    
    if (str_strcmp(argv[0], "clear") == 0) {
        clear();
        return 0;
    }

    if (str_strcmp(argv[0], "ver") == 0) {
        println("Experimental DogeShell");
        return 0;
    }

    if (str_strcmp(argv[0], "help") == 0) {
        help_me();
        return 0;
    }

    else {
        println("command not found");
        return -2;
    }
    return -127;
}

int tokenize(char* command, int argc_max, char** argv_buf) {
    int argc_count = 0;
    char* start = NULL;
    int len = 0;

    for (int i = 0; command[i] != '\0'; i++) {
        if (command[i] != ' ') {
            if (start == NULL) {
                start = &command[i];
                len = 0;
            }
            len++;
        } else {
            if (start != NULL) {
                if (argc_count < argc_max) {
                    memcpy(argv_buf[argc_count], start, (size_t)len);
                    argv_buf[argc_count][len] = '\0';
                    argc_count++;
                }
                start = NULL;
            }
        }
    }

    if (start != NULL && argc_count < argc_max) {
        memcpy(argv_buf[argc_count], start, (size_t)len);
        argv_buf[argc_count][len] = '\0';
        argc_count++;
    }

    return argc_count;
}

void _start(void) {
    println("Welcome to WindogeOS DogeShell experimental shell.");
    println("It's just so i can try and replace kernel space dogeshell, don't expect\
        \nit to be fully featured like the kernel space one (default).");

    char command[512];
    char prompt[96];
    
    char arguments[MAX_ARGS][ARG_SIZE];
    char* argv_buffer[MAX_ARGS];
    
    for (int i = 0; i < MAX_ARGS; i++) {
        argv_buffer[i] = arguments[i];
    }

    while (true) {
        str_strcpy(prompt, "experimental WindogeOS > ");
        input(prompt, command, 256);
        
        
        int argc_actual = tokenize(command, MAX_ARGS, argv_buffer);
        
        execute(argc_actual, argv_buffer);
    }
    exit((uint64_t)-1);
}
