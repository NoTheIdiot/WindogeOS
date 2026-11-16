#include "../../library/stddef.h"
#include "../../library/stdint.h"
#include "../../library/dogeio.h"
#include "../../library/dogelib.h"
#include "../../library/string.h"
#include "../../library/bool.h"

#define CMD_BUF_SIZE 256
#define MAX_ARGS     16
#define MAX_DIR_ENTRIES 24
#define NUM_BUF_SIZE 64
#define READ_BUF_SIZE 8192

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

    if (strcmp(argv[0], "create") == 0) {
        if (argv[1] == NULL) {
            println("Need a filename.");
            return -1;
        }
        if (!(bool)fs_exists(argv[1])) {
            println("File already exists.");
            return -1;
        }
        if (fs_create(argv[1]) != 0) {
            println("Failed to create file.");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "del") == 0) {
        if (argv[1] == NULL) {
            println("Need one argument.");
            return -1;
        }

        if (fs_exists(argv[1])) {
            println("File doesn't exist");
            return -1;
        }
        fs_delete(argv[1]);
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

    if (strcmp(argv[0], "dir") == 0) {
        static fs_t files[MAX_DIR_ENTRIES];
        memset(files, 0, sizeof(files));

        char buffer[64];
        fs_get_cwd(buffer, 64);
        int count = fs_list(buffer, files, MAX_DIR_ENTRIES);

        if (count < 0) {
            println("Filesystem error or not mounted.");
            return -1;
        }

        if (count == 0) {
            println("Directory is empty.");
            return 0;
        }

        if (count > MAX_DIR_ENTRIES) {
            count = MAX_DIR_ENTRIES;
        }

        char size_str[NUM_BUF_SIZE];
        for (int i = 0; i < count; i++) {
            if (files[i].name[0] == '\0') {
                continue;
            }

            if (files[i].is_dir) {
                print("[DIR]  ");
            } else {
                print("[FILE] ");
            }

            print(files[i].name);
            print("  (");
            itoa((int)files[i].size, size_str);
            print(size_str);
            println(" bytes)");
        }
        return 0;
    }

    if (strcmp(argv[0], "cd") == 0) {
        if (argv[1] == NULL) {
            println("cd: cd <path>");
            return -1;
        }
        if (fs_chdir(argv[1]) == -1) {
            println("Not Wow: Permission denied :(");
        }
        return 0;
    }

    if (strcmp(argv[0], "shutdown") == 0) {
        windoge_shutdown();
        return 0;
    }

    if (strcmp(argv[0], "reboot") == 0) {
        windoge_reboot();
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
    char buffer[32];

    while (1) {
        fs_get_cwd(buffer, 32);
        print(buffer);
        if (input("> ", command, sizeof(command)) < 0) {
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
