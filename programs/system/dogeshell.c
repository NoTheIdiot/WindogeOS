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
#define HISTORY_SIZE     12
#define PROGRAMS_DIR     "/programs"

#define HISTORY_SIZE     12
#define HISTORY_FILE     ".history"
#define HISTORY_BUF_SIZE (HISTORY_SIZE * CMD_BUF_SIZE)

typedef struct {
    char entries[HISTORY_SIZE][CMD_BUF_SIZE];
    int count;
    int head;
} cmd_history_t;

static cmd_history_t g_history;

static void clean_input_string(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                       s[len - 1] == ' '  || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static void copy_str_bounded(char *dst, const char *src, size_t cap) {
    if (!dst || cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t i = 0;
    while (i + 1 < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool has_path_prefix(const char *name) {
    if (!name) return false;
    for (size_t i = 0; name[i] != '\0'; i++) {
        if (name[i] == '/') {
            return true;
        }
    }
    return false;
}

static int run_program_file(const char *path) {
    if (!path || path[0] == '\0') {
        return -1;
    }
    int64_t size = fs_size(path);
    if (size < 0) {
        return -1;
    }
    if (exec_prog(path, (uint64_t)size) < 0) {
        printf("Failed to run program: %s\n", path);
        return -1;
    }
    return 0;
}

static int history_load_lines(char entries[HISTORY_SIZE][CMD_BUF_SIZE]) {
    int64_t size = fs_size(HISTORY_FILE);
    if (size <= 0) {
        return 0;
    }

    char raw[HISTORY_BUF_SIZE];
    size_t to_read = (size >= HISTORY_BUF_SIZE) ? (HISTORY_BUF_SIZE - 1) : (size_t)size;
    int64_t n = fs_read(HISTORY_FILE, raw, to_read);
    if (n <= 0) {
        return 0;
    }
    raw[n] = '\0';

    int count = 0;
    size_t pos = 0;
    while (pos < (size_t)n && count < HISTORY_SIZE) {
        size_t out = 0;
        while (pos < (size_t)n && raw[pos] != '\n' && raw[pos] != '\r') {
            if (out + 1 < CMD_BUF_SIZE) {
                entries[count][out++] = raw[pos];
            }
            pos++;
        }
        entries[count][out] = '\0';
        if (out > 0) {
            count++;
        }
        while (pos < (size_t)n && (raw[pos] == '\n' || raw[pos] == '\r')) {
            pos++;
        }
    }

    return count;
}

static void history_save_lines(char entries[HISTORY_SIZE][CMD_BUF_SIZE], int start, int count) {
    char raw[HISTORY_BUF_SIZE];
    size_t pos = 0;

    for (int i = 0; i < count; i++) {
        const char *line = entries[start + i];
        for (size_t j = 0; line[j] != '\0' && pos + 2 < HISTORY_BUF_SIZE; j++) {
            raw[pos++] = line[j];
        }
        if (pos + 1 < HISTORY_BUF_SIZE) {
            raw[pos++] = '\n';
        }
    }
    raw[pos] = '\0';

    if (fs_size(HISTORY_FILE) < 0) {
        fs_create(HISTORY_FILE);
    }
    fs_write(HISTORY_FILE, raw, pos);
}

static void history_push(const char *line) {
    if (!line || line[0] == '\0') return;

    char entries[HISTORY_SIZE][CMD_BUF_SIZE];
    int count = history_load_lines(entries);

    if (count > 0 && strcmp(entries[count - 1], line) == 0) {
        return;
    }

    if (count < HISTORY_SIZE) {
        copy_str_bounded(entries[count], line, CMD_BUF_SIZE);
        count++;
        history_save_lines(entries, 0, count);
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++) {
            copy_str_bounded(entries[i - 1], entries[i], CMD_BUF_SIZE);
        }
        copy_str_bounded(entries[HISTORY_SIZE - 1], line, CMD_BUF_SIZE);
        history_save_lines(entries, 0, HISTORY_SIZE);
    }
}

static void history_print(void) {
    char entries[HISTORY_SIZE][CMD_BUF_SIZE];
    int count = history_load_lines(entries);

    for (int i = 0; i < count; i++) {
        printf("%d  %s\n", i + 1, entries[i]);
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

    if (!line || !argv || argc <= 1) {
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
        argv[count] = dst;
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
        count++;
    }

    argv[count] = NULL;
    return count;
}

static void print_help(void) {
    printf("Built-in commands:\n");
    printf("  help                Show this help message\n");
    printf("  echo | print ...    Print text to console\n");
    printf("  clear               Clear terminal screen\n");
    printf("  ls | dir [path]     List directory entries\n");
    printf("  cd <path>           Change current directory\n");
    printf("  whereami            Print current working directory\n");
    printf("  mkdir <path>        Create a directory\n");
    printf("  create <file>       Create an empty file\n");
    printf("  del | rm <path>     Delete a file\n");
    printf("  read | cat <file>   Print file contents\n");
    printf("  write <file> <str>  Overwrite file with text\n");
    printf("  append <file> <str> Append text to file\n");
    printf("  stat <path>         Show entry type and size\n");
    printf("  exists <path>       Check if entry exists\n");
    printf("  run <path>          Execute a program file\n");
    printf("  sleep <ms>          Sleep for milliseconds\n");
    printf("  beep <hz> <ms>      Play tone\n");
    printf("  uptime              Show system uptime\n");
    printf("  time                Show current system time\n");
    printf("  rand [min] [max]    Generate a random number\n");
    printf("  history             Show command history\n");
    printf("  exit                Exit shell\n");
    printf("External programs in %s are also executed automatically.\n", PROGRAMS_DIR);
}

int execute(int argc, char **argv) {
    if (argc <= 0 || argv == NULL || argv[0] == NULL) {
        return -1;
    }

    if (strcmp(argv[0], "help") == 0) {
        print_help();
        return 0;
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
        const char *path = (argc > 1) ? argv[1] : ".";
        fs_t entries[MAX_DIR_ENTRIES];
        int count = fs_list(path, entries, MAX_DIR_ENTRIES);

        if (count < 0) {
            printf("Failed to list directory: %s\n", path);
            return -1;
        }

        for (int i = 0; i < count; i++) {
            char size_buf[NUM_BUF_SIZE];
            fmt_u64(entries[i].size, size_buf, sizeof(size_buf));

            if (entries[i].is_dir) {
                printf("<DIR>  %s\n", entries[i].name);
            } else {
                printf("%s B  %s\n", size_buf, entries[i].name);
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
            printf("Directory does not exist: %s\n", argv[1]);
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "whereami") == 0) {
        char cwd[CMD_BUF_SIZE];
        memset(cwd, 0, sizeof(cwd));
        if (fs_get_dir(cwd, sizeof(cwd)) != 0) {
            printf("Failed to get current directory.\n");
            return -1;
        }
        printf("%s\n", cwd);
        return 0;
    }

    if (strcmp(argv[0], "mkdir") == 0) {
        if (argc < 2) {
            printf("Usage: mkdir <path>\n");
            return -1;
        }
        if (fs_exists(argv[1])) {
            printf("Entry already exists.\n");
            return -1;
        }
        if (fs_mkdir(argv[1]) != 0) {
            printf("Failed to create directory.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "create") == 0) {
        if (argc < 2) {
            printf("Usage: create <filename>\n");
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

    if (strcmp(argv[0], "del") == 0 || strcmp(argv[0], "rm") == 0) {
        if (argc < 2) {
            printf("Usage: %s <path>\n", argv[0]);
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
        if (argc < 2) {
            printf("Usage: sleep <ms>\n");
            return -1;
        }
        wait(parse_u64(argv[1]));
        return 0;
    }

    if (strcmp(argv[0], "run") == 0) {
        if (argc < 2) {
            printf("Usage: run <path>\n");
            return -1;
        }
        if (fs_size(argv[1]) < 0) {
            printf("Program/file doesn't exist.\n");
            return -1;
        }
        return run_program_file(argv[1]);
    }

    if (strcmp(argv[0], "read") == 0 || strcmp(argv[0], "cat") == 0) {
        if (argc < 2) {
            printf("Usage: %s <filename>\n", argv[0]);
            return -1;
        }
        if (!fs_exists(argv[1])) {
            printf("File doesn't exist.\n");
            return -1;
        }
        static char buffer[READ_BUF_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int64_t bytes = fs_read_all(argv[1], buffer, sizeof(buffer) - 1);
        if (bytes >= 0) {
            if (bytes >= (int64_t)sizeof(buffer)) {
                bytes = (int64_t)sizeof(buffer) - 1;
            }
            buffer[bytes] = '\0';
            printf("%s\n", buffer);
        } else {
            printf("Failed to read file.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "write") == 0) {
        if (argc < 3) {
            printf("Usage: write <filename> <text>\n");
            return -1;
        }
        size_t len = strlen(argv[2]);
        if (fs_write_all(argv[1], argv[2], (uint64_t)len) != 0) {
            printf("Failed to write file.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "append") == 0) {
        if (argc < 3) {
            printf("Usage: append <filename> <text>\n");
            return -1;
        }
        size_t len = strlen(argv[2]);
        if (fs_append(argv[1], argv[2], (uint64_t)len) != 0) {
            printf("Failed to append to file.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "stat") == 0) {
        if (argc < 2) {
            printf("Usage: stat <path>\n");
            return -1;
        }
        doge_fs_stat_t st = {0};
        if (fs_stat(argv[1], &st) != 0) {
            printf("Entry doesn't exist.\n");
            return -1;
        }
        char size_str[NUM_BUF_SIZE];
        fmt_u64(st.size, size_str, sizeof(size_str));
        printf("%s  (%s bytes)\n", argv[1], size_str);
        return 0;
    }

    if (strcmp(argv[0], "exists") == 0) {
        if (argc < 2) {
            printf("Usage: exists <path>\n");
            return -1;
        }
        if (!fs_exists(argv[1])) {
            printf("Does not exist.\n");
        } else {
            printf("Exists.\n");
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

    if (strcmp(argv[0], "beep") == 0) {
        if (argc < 3) {
            printf("Usage: beep <hz> <ms>\n");
            return -1;
        }
        beep(parse_u64(argv[1]), parse_u64(argv[2]));
        return 0;
    }

    if (strcmp(argv[0], "uptime") == 0) {
        char buf[NUM_BUF_SIZE];
        fmt_u64(uptime(), buf, sizeof(buf));
        printf("uptime: %s\n", buf);
        return 0;
    }

    if (strcmp(argv[0], "time") == 0) {
        char tbuf[128];
        memset(tbuf, 0, sizeof(tbuf));
        if (get_time_str(tbuf, sizeof(tbuf)) < 0) {
            printf("Failed to get time.\n");
            return -1;
        }
        printf("%s\n", tbuf);
        return 0;
    }

    if (strcmp(argv[0], "rand") == 0) {
        uint64_t val = 0;
        if (argc == 1) {
            val = rand_u64();
        } else if (argc == 2) {
            uint64_t raw_max = parse_u64(argv[1]);
            uint32_t max_v = (raw_max > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw_max;
            val = rand_range(0, max_v);
        } else {
            uint64_t raw_min = parse_u64(argv[1]);
            uint64_t raw_max = parse_u64(argv[2]);
            uint32_t min_v = (raw_min > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw_min;
            uint32_t max_v = (raw_max > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw_max;
            val = rand_range(min_v, max_v);
        }
        char rbuf[NUM_BUF_SIZE];
        fmt_u64(val, rbuf, sizeof(rbuf));
        printf("%s\n", rbuf);
        return 0;
    }

    if (strcmp(argv[0], "history") == 0) {
        history_print();
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0) {
        printf("exiting\n");
        exit();
        return 0;
    }

    char prog_path[CMD_BUF_SIZE];
    if (has_path_prefix(argv[0])) {
        copy_str_bounded(prog_path, argv[0], sizeof(prog_path));
        if (fs_size(prog_path) >= 0) {
            return run_program_file(prog_path);
        }
    } else {
        size_t dir_len = strlen(PROGRAMS_DIR);
        size_t name_len = strlen(argv[0]);
        if (dir_len + 1 + name_len + 1 <= sizeof(prog_path)) {
            copy_str_bounded(prog_path, PROGRAMS_DIR, sizeof(prog_path));
            prog_path[dir_len] = '/';
            copy_str_bounded(prog_path + dir_len + 1, argv[0], sizeof(prog_path) - dir_len - 1);
            int64_t prog_size = fs_size(prog_path);
            if (prog_size >= 0) {
                return run(prog_path, (size_t)prog_size, argc, argv);
            }
        }
    }

    printf("%s: command does not exist. Type 'help' for commands.\n", argv[0]);
    return -128;
}

static void print_prompt(void) {
    char cwd[CMD_BUF_SIZE];
    memset(cwd, 0, sizeof(cwd));
    if (fs_get_dir(cwd, sizeof(cwd)) == 0 && cwd[0] != '\0') {
        print_str(cwd);
    } else {
        print_str("/");
    }
    print_str("> ");
}

void _start(void) {
    char command[CMD_BUF_SIZE];
    char *argv[MAX_ARGS];
    printf("\033[096mWindogeOS Dogeshell v0.4\033[0m\n");

    while (1) {
        print_prompt();
        if (input(command, sizeof(command)) < 0) {
            continue;
        }

        clean_input_string(command);
        if (command[0] == '\0') {
            continue;
        }

        history_push(command);

        int argc = shell_tokenize(command, argv, MAX_ARGS);
        if (argc < 0) {
            printf("Syntax error: unclosed quote.\n");
            continue;
        }

        execute(argc, argv);
    }
}