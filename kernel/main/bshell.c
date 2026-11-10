#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <system.h>
#include <bool.h>
#include <syscalls.h>

int shell_tokenize(char *line, char **argv, int argc) {
    int count = 0;
    char *p = line;

    if (!line || !argv || argc <= 0) {
        return 0;
    }

    // split from spaces
    while (*p != '\0' && count < argc - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (*p == '"') {
            p++;
            argv[count++] = p;

            while (*p != '\0' && *p != '"') {
                p++;
            }

            if (*p == '"') {
                *p = '\0';
                p++;
            }
        } else {
            argv[count++] = p;

            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                p++;
            }

            if (*p != '\0') {
                *p = '\0';
                p++;
            }
        }
    }

    argv[count] = NULL;
    return count;
}

int execute(int argc, char** argv) {
    // check for NULL because otherwise whole system crashes
    if (argc <= 0) {
        return -1;
    }

    if (argv == NULL) {
        return -1;
    }

    // actual commands
    if (strcmp(argv[0], "print") == 0) {
        if (argc == 1) {
            println("");
        } else {
            println(argv[1]);
        }
        return 0;
    }

    if (strcmp(argv[0], "clear") == 0) {
        clear();
        return 0;
    }

    if (strcmp(argv[0], "dir") == 0) {
        fs_t files[256];
        int count = fs_list(files, 256);
        
        if (count < 0) {
            println("Filesystem error or not mounted.");
            return -1;
        }

        for (int i = 0; i < count; i++) {
            println(files[i].name);
        }
        return 0;
    }

    if (strcmp(argv[0], "reboot") == 0) {
        reboot();
        return 0;
    }

    if (strcmp(argv[0], "shutdown") == 0) {
        shutdown();
        return 0;
    }

    if (strcmp(argv[0], "read") == 0) {
        if (argv[1] == NULL) {
            println("Need one argument.");
            return -1;
        }
        if (!fs_exists(argv[1])) {
            println("File doesn't exist.");
            return -1;
        }
        char buffer[8192];
        fs_read(argv[1], buffer, 8192);
        println(buffer);
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0 ||
        strcmp(argv[0], "kys")  == 0) {
        
        println("Returning back, will halt phooking forever.");
        return 500;
    }

    print(argv[0]);
    println(": command does not much exist.");
    return -128;
}

void backup_shell(void) {
    char command[256];
    char *argv[16];
    int argc;
    while (true) {
        input("root> ", command);
        argc = shell_tokenize(command, argv, 16);
        int result = execute(argc, argv);

        if (result == 500) {
            return;
        }
    }
}