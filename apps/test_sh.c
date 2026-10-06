#include "../library/dogeio.h"
#include "../library/string.h"

void _start(void) {
    char command[64] = {0};
    while (1) {
        command[0] = '\0';
        if (input("sh> ", command, sizeof(command)) == (uint64_t)-1) {
            println("input failed");
            continue;
        }
        command[sizeof(command) - 1] = '\0';

        if (str_startswith(command, "echo")) {
            println(command + 6);
        }
        
        else if (str_startswith(command, "ver")) {
            println("sh v0.1");
        } 

        else if (str_startswith(command, "clear")) {
            clear();
        }
        
        else if (str_strcmp(command, "exit") == 0) {
            sys_exit(0);
        }
        
        else {
            println("command doesn't exit");
        }

    }
    sys_exit(0);
}