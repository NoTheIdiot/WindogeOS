#include "../library/stddef.h"
#include "../library/stdint.h"
#include "../library/dogeio.h"
#include "../library/string.h"

#define CMD_BUF_SIZE 256
#define MAX_ARGS     16

int shell_tokenize(char *line, char **argv, int argc) {
    int count = 0;
    char *src = line;

    if (!line || !argv || argc <= 0) {
        return 0;
    }

    while (*src != '\0' && count < argc - 1) {
        while (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') {
            src++;
        }

        if (*src == '\0') {
            break;
        }

        char *dst = src;
        argv[count++] = dst;
        char quote = '\0';

        while (*src != '\0') {
            if (quote == '\0') {
                if (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') {
                    src++;
                    break;
                }
                if (*src == '"' || *src == '\'') {
                    quote = *src++;
                    continue;
                }
                if (*src == '\\' && *(src + 1) != '\0') {
                    src++;
                    *dst++ = *src++;
                    continue;
                }
                *dst++ = *src++;
            } else {
                if (*src == quote) {
                    quote = '\0';
                    src++;
                    continue;
                }
                if (quote == '"' && *src == '\\' && *(src + 1) != '\0') {
                    src++;
                    if (*src == 'n') {
                        *dst++ = '\n';
                    } else if (*src == 't') {
                        *dst++ = '\t';
                    } else {
                        *dst++ = *src;
                    }
                    src++;
                    continue;
                }
                *dst++ = *src++;
            }
        }

        if (quote != '\0') {
            return -1;
        }

        *dst = '\0';
    }

    argv[count] = NULL;
    return count;
}

int execute(int argc, char **argv) {
    if (argc <= 0 || argv == NULL) {
        return -1;
    }

    if (strcmp(argv[0], "print") == 0 || strcmp(argv[0], "echo") == 0) {
        if (argc == 1) {
            println("");
        } else {
            for (int i = 1; i < argc; i++) {
                print(argv[i]);
                if (i + 1 < argc) {
                    print(" ");
                }
            }
            println("");
        }
        return 0;
    }

    if (strcmp(argv[0], "clear") == 0) {
        clear();
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0) {
        println("exiting");
        exit();
        return 0;
    }

    print(argv[0]);
    println(": command does not exist.");
    return -128;
}

void _start(void) {
    char command[CMD_BUF_SIZE];
    char *argv[MAX_ARGS];

    while (1) {
        if (input("doge> ", command, sizeof(command)) < 0) {
            continue;
        }

        clean_input_string(command);
        if (command[0] == '\0') {
            continue;
        }

        int argc = shell_tokenize(command, argv, MAX_ARGS);
        if (argc < 0) {
            println("Syntax error: unclosed quote.");
            continue;
        }

        execute(argc, argv);
    }
}
