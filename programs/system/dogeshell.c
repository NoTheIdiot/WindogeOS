#include "../../dogec/stddef.h"
#include "../../dogec/stdint.h"
#include "../../dogec/dogeio.h"
#include "../../dogec/string.h"
#include "../../dogec/bool.h"

#define CMD_BUF_SIZE     256
#define MAX_ARGS         16
#define MAX_DIR_ENTRIES  24
#define NUM_BUF_SIZE     64
#define READ_BUF_SIZE    8192

static void clean_input_string(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                       s[len - 1] == ' '  || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static uint64_t parse_u64(const char *s) {
    uint64_t val = 0;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++;
    while (*s >= '0' && *s <= '9') {
        val = val * 10ULL + (uint64_t)(*s - '0');
        s++;
    }
    return val;
}

static void fmt_u64(uint64_t val, char *buf, size_t cap) {
    if (!buf || cap == 0) return;
    if (val == 0) {
        if (cap >= 2) {
            buf[0] = '0';
            buf[1] = '\0';
        } else {
            buf[0] = '\0';
        }
        return;
    }

    char tmp[32];
    int idx = 0;
    while (val > 0 && idx < (int)sizeof(tmp)) {
        tmp[idx++] = (char)('0' + (val % 10ULL));
        val /= 10ULL;
    }

    size_t out = 0;
    while (idx > 0 && out + 1 < cap) {
        buf[out++] = tmp[--idx];
    }
    buf[out] = '\0';
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

int execute(int argc, char **argv) {
    if (argc <= 0 || argv == NULL || argv[0] == NULL) {
        return -1;
    }

    if (strcmp(argv[0], "print") == 0 || strcmp(argv[0], "echo") == 0) {
        if (argc == 1) {
            printf("\n");
        } else {
            for (int i = 1; i < argc; i++) {
                print_str(argv[i]);
                if (i + 1 < argc) {
                    print_str(" ");
                }
            }
            printf("\n");
        }
        return 0;
    }

    if (strcmp(argv[0], "clear") == 0) {
        clear();
        return 0;
    }

    if (strcmp(argv[0], "dir") == 0 || strcmp(argv[0], "ls") == 0) {
        const char *path = ".";

        if (argc > 1) {
            path = argv[1];
        }

        fs_t entries[MAX_DIR_ENTRIES];

        int count = fs_list((char *)path, entries, MAX_DIR_ENTRIES);

        if (count < 0) {
            printf("Failed to list directory.\n");
            return -1;
        }

        for (int i = 0; i < count; i++) {

            char size_buf[NUM_BUF_SIZE];
            fmt_u64(entries[i].size, size_buf, sizeof(size_buf));

            if (entries[i].is_dir) {
                printf("<DIR> %s\n", entries[i].name);
            } else {
                printf("%s bytes %s\n", size_buf, entries[i].name);
            }
        }

        return 0;
    }

    if (strcmp(argv[0], "cd") == 0) {
        if (argc < 2) {
            printf("Usage: cd <path>\n");
            return -1;
        }

        if (fs_chdir(argv[1]) != 0) {
            printf("Directory does not exist.\n");
            return -1;
        }

        return 0;
    }

    if (strcmp(argv[0], "whereami") == 0) {
        char cwd[256];

        memset(cwd, 0, sizeof(cwd));

        fs_get_dir(cwd, sizeof(cwd));

        printf("%s\n", cwd);

        return 0;
    }

    if (strcmp(argv[0], "create") == 0) {
        if (argv[1] == NULL) {
            printf("Need a filename.\n");
            return -1;
        }
        if (fs_exists(argv[1])) {
            printf("File already exists.\n");
            return -1;
        }
        if (fs_create(argv[1]) != 0) {
            printf("Failed to create file.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "del") == 0) {
        if (argv[1] == NULL) {
            printf("Need one argument.\n");
            return -1;
        }
        if (!fs_exists(argv[1])) {
            printf("File doesn't exist.\n");
            return -1;
        }
        if (fs_delete(argv[1]) != 0) {
            printf("Failed to delete file.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "sleep") == 0) {
        if (argv[1] == NULL) {
            printf("usage: sleep <ms>\n");
            return -1;
        }
        wait(parse_u64(argv[1]));
        return 0;
    }

    if (strcmp(argv[0], "run") == 0) {
        if (argv[1] == NULL) {
            printf("run: run <path> [args...]\n");
            return -1;
        }
        int64_t size = fs_size(argv[1]);
        if (size < 0) {
            printf("Program/file doesn't exist.\n");
            return -1;
        }
        if (exec_prog(argv[1], (uint64_t)size) < 0) {
            printf("Failed to run program.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "read") == 0) {
        if (argv[1] == NULL) {
            printf("Need one argument.\n");
            return -1;
        }
        if (!fs_exists(argv[1])) {
            printf("File doesn't exist.\n");
            return -1;
        }
        static char buffer[READ_BUF_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int64_t bytes = fs_read(argv[1], buffer, sizeof(buffer) - 1);
        if (bytes >= 0) {
            buffer[bytes] = '\0';
            printf("%s\n", buffer);
        } else {
            printf("Failed to read file.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "write") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) {
            printf("Need two arguments.\n");
            return -1;
        }
        if (!fs_exists(argv[1])) {
            printf("File doesn't exist.\n");
            return -1;
        }
        size_t len = strlen(argv[2]);
        int64_t written = fs_write(argv[1], argv[2], (uint64_t)len);
        if (written < 0 || (size_t)written != len) {
            printf("Failed to write file.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "stat") == 0) {
        if (argv[1] == NULL) {
            printf("Usage: stat <path>\n");
            return -1;
        }
        doge_fs_stat_t st = {0};
        if (fs_stat(argv[1], &st) != 0) {
            printf("File doesn't exist.\n");
            return -1;
        }
        char size_str[NUM_BUF_SIZE];
        fmt_u64(st.size, size_str, sizeof(size_str));
        print_str(argv[1]);
        printf("  (%s bytes)\n", size_str);
        return 0;
    }

    if (strcmp(argv[0], "exists") == 0) {
        if (argv[1] == NULL) {
            printf("Usage: exists <thing>\n");
            return -1;
        }
        if (!fs_exists(argv[1])) {
            printf("File doesn't exist.\n");
        } else {
            printf("File exists.\n");
        }
        return 0;
    }

    if (strcmp(argv[0], "beep") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) {
            printf("Usage: beep <hz> <time>\n");
            return 0;
        }
        beep(parse_u64(argv[1]), parse_u64(argv[2]));
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0) {
        printf("exiting\n");
        exit();
        return 0;
    }

    print_str(argv[0]);
    printf(": command does not exist.\n");
    return -128;
}

void _start(void) {
    char command[CMD_BUF_SIZE];
    char *argv[MAX_ARGS];

    while (1) {
        print_str("/");
        print_str("> ");
        if (input(command, sizeof(command)) < 0) {
            continue;
        }

        clean_input_string(command);
        if (command[0] == '\0') {
            continue;
        }

        int argc = shell_tokenize(command, argv, MAX_ARGS);
        if (argc < 0) {
            printf("Syntax error: unclosed quote.\n");
            continue;
        }

        execute(argc, argv);
    }
}