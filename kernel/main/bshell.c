#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <system.h>
#include <bool.h>
#include <syscalls.h>

#define READ_BUF_SIZE 8192
#define CMD_BUF_SIZE  256
#define MAX_ARGS      16

int shell_tokenize(char *line, char **argv, int argc) {
    int count = 0;
    char *p = line;

    if (!line || !argv || argc <= 0) {
        return 0;
    }

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
    if (argc <= 0 || argv == NULL) {
        return -1;
    }

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

    if (strcmp(argv[0], "reboot") == 0) {
        reboot();
        return 0;
    }

    if (strcmp(argv[0], "shutdown") == 0) {
        shutdown();
        return 0;
    }

        if (strcmp(argv[0], "create") == 0) {
            if (argv[1] == NULL) {
                println("Need a filename.");
                return -1;
            }
            if (fs_exists(argv[1])) {
                println("File already exists.");
                return -1;
            }
            if (fs_create(argv[1]) != 0) {
                println("Failed to create file.");
                return -1;
            }
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
        static char buffer[READ_BUF_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int bytes = fs_read(argv[1], buffer, sizeof(buffer) - 1);
        if (bytes >= 0) {
            buffer[bytes] = '\0';
            println(buffer);
        } else {
            println("Failed to read file.");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "write") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) {
            println("Need two arguments.");
            return -1;
        }
        if (!fs_exists(argv[1])) {
            println("File doesn't exist.");
            return -1;
        }
        if (fs_write(argv[1], argv[2], strlen(argv[2])) != 0) {
            println("Failed to write file.");
            return -1;
        }
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
    char command[CMD_BUF_SIZE];
    char *argv[MAX_ARGS];
    int argc;
    while (true) {
        input("root> ", command);
        argc = shell_tokenize(command, argv, MAX_ARGS);
        int result = execute(argc, argv);

        if (result == 500) {
            return;
        }
    }
}
