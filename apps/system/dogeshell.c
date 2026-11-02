#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/string.h"
#include "../library/stddef.h"
#include "../library/stdint.h"
#include "../library/bool.h"

int execute(char* argv) {
    if (str_strcmp(argv, "exit") == 0) {
        exit(0);
    }
    
    if (str_startswith(argv, "print")) {
        println(argv + 6);
        return 0;
    }
    
    if (str_strcmp(argv, "clear") == 0) {
        clear();
        return 0;
    }

    else {
        println("command not found");
        return -2;
    }
    return -127;
}

void _start(void) {
    char command[256];
    char prompt[96];
    while (true) {
        str_strcpy(prompt, "experimental WindogeOS > ");

        input(prompt, command, 256);
        execute(command);
    }
    exit((uint64_t)-1);
}
