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

#define MAX_DIR_ENTRIES 128
#define NUM_BUF_SIZE    16

static void uint32_to_str(uint32_t val, char *buf) {
    if (val == 0) {
        *(buf + 0) = '0';
        *(buf + 1) = '\0';
        return;
    }

    char temp[NUM_BUF_SIZE];
    int i = 0;
    while (val > 0) {
        *(temp + i) = (char)('0' + (val % 10));
        i++;
        val /= 10;
    }

    int j = 0;
    while (i > 0) {
        i--;
        *(buf + j) = *(temp + i);
        j++;
    }
    *(buf + j) = '\0';
}

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

int execute(int argc, char** argv) {
    if (argc <= 0 || argv == NULL) {
        return -1;
    }

    if (strcmp(argv[0], "print") == 0) {
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
        if (!fs_exists(argv[1])) {
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

    if (strcmp(argv[0], "del") == 0) {
        if (argv[1] == NULL) {
            println("Need one argument.");
            return -1;
        }

        if (!fs_exists(argv[1])) {
            println("File doesn't exist");
            return -1;
        }
        fs_delete(argv[1]);
        return 0;
    }

    if (strcmp(argv[0], "dir") == 0) {
        static fs_t files[MAX_DIR_ENTRIES];
        int count = fs_list(files, MAX_DIR_ENTRIES);

        if (count < 0) {
            println("Filesystem error or not mounted.");
            return -1;
        }

        if (count == 0) {
            println("Directory is empty.");
            return 0;
        }

        char size_str[NUM_BUF_SIZE];
        for (int i = 0; i < count; i++) {
            if (files[i].is_dir) {
                print("[DIR]  ");
            } else {
                print("[FILE] ");
            }

            print(files[i].name);
            print("  (");
            uint32_to_str(files[i].size, size_str);
            print(size_str);
            println(" bytes)");
        }
        return 0;
    }

    if (strcmp(argv[0], "overwrite") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) {
            println("Need two arguments.");
            return -1;
        }
        if (!fs_exists(argv[1])) {
            println("File doesn't exist.");
            return -1;
        }
        if (fs_write_force(argv[1], argv[2], strlen(argv[2])) != 0) {
            println("Failed to write file.");
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

    if (strcmp(argv[0], "test1") == 0) {
        run_elf("program", 8192);
        return 0;
    }

    if (strcmp(argv[0], "test2") == 0) {
        speaker_beep(500, 500);
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
        if (argc < 0) {
            println("Syntax error: unclosed quote.");
            continue;
        }
        int result = execute(argc, argv);

        if (result == 500) {
            return;
        }
    }
}
