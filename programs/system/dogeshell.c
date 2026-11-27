#include "../../dogec/stddef.h"
#include "../../dogec/stdint.h"
#include "../../dogec/dogeio.h"
#include "../../dogec/string.h"
#include "../../dogec/stdio.h"
#include "../../dogec/bool.h"

#define CMD_BUF_SIZE    256
#define MAX_ARGS        16
#define MAX_DIR_ENTRIES 24
#define NUM_BUF_SIZE    64
#define READ_BUF_SIZE   8192
#define TAIL_MAX_LINES  32
#define HISTORY_SIZE    12
#define HISTORY_FILE    ".history"
#define PROGRAMS_DIR    "/programs"

typedef int (*builtin_fn_t)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *alias;
    const char *usage;
    builtin_fn_t fn;
} builtin_cmd_t;

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

static int parse_int_default(const char *s, int fallback) {
    if (!s || *s == '\0') return fallback;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') return fallback;

    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if (*s < '0' || *s > '9') return fallback;

    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val * sign;
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

static int run_program_file(const char *path, int argc, char **argv) {
    if (!path || path[0] == '\0') {
        return -1;
    }
    int64_t size = fs_size(path);
    if (size < 0) {
        return -1;
    }
    if (argc > 0 && argv != NULL) {
        if (run(path, (size_t)size, argc, argv) < 0) {
            printf("Failed to run program: %s\n", path);
            return -1;
        }
        return 0;
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

    char raw[HISTORY_SIZE * CMD_BUF_SIZE + 1];
    size_t max_raw = sizeof(raw) - 1;
    size_t to_read = ((size_t)size >= max_raw) ? max_raw : (size_t)size;
    int64_t n = fs_read(HISTORY_FILE, raw, to_read);
    if (n <= 0) {
        return 0;
    }
    if ((size_t)n > max_raw) {
        n = (int64_t)max_raw;
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

static void history_save_lines(char entries[HISTORY_SIZE][CMD_BUF_SIZE], int count) {
    char raw[HISTORY_SIZE * CMD_BUF_SIZE + 1];
    size_t pos = 0;

    if (count < 0) count = 0;
    if (count > HISTORY_SIZE) count = HISTORY_SIZE;

    for (int i = 0; i < count; i++) {
        const char *line = entries[i];
        for (size_t j = 0; line[j] != '\0' && pos + 2 < sizeof(raw); j++) {
            raw[pos++] = line[j];
        }
        if (pos + 1 < sizeof(raw)) {
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
        history_save_lines(entries, count);
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++) {
            copy_str_bounded(entries[i - 1], entries[i], CMD_BUF_SIZE);
        }
        copy_str_bounded(entries[HISTORY_SIZE - 1], line, CMD_BUF_SIZE);
        history_save_lines(entries, HISTORY_SIZE);
    }
}

static void history_print(void) {
    char entries[HISTORY_SIZE][CMD_BUF_SIZE];
    int count = history_load_lines(entries);

    for (int i = 0; i < count; i++) {
        printf("%d  %s\n", i + 1, entries[i]);
    }
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

static int copy_file_data(const char *src, const char *dst) {
    if (!src || !dst) return -1;
    if (strcmp(src, dst) == 0) {
        printf("Source and destination are identical.\n");
        return -1;
    }
    if (!fs_exists(src)) {
        printf("Source file doesn't exist: %s\n", src);
        return -1;
    }

    static char buf[READ_BUF_SIZE];
    int64_t n = fs_read_all(src, buf, sizeof(buf));
    if (n < 0) {
        printf("Failed to read source file: %s\n", src);
        return -1;
    }
    if (n >= (int64_t)sizeof(buf)) {
        printf("File too large for buffer (%s).\n", src);
        return -1;
    }

    if (!fs_exists(dst) && fs_create(dst) != 0) {
        printf("Failed to create destination file: %s\n", dst);
        return -1;
    }

    if (fs_write_all(dst, buf, (uint64_t)n) != 0) {
        printf("Failed to write destination file: %s\n", dst);
        return -1;
    }
    return 0;
}

static int b_help(int argc, char **argv);
static int b_print(int argc, char **argv);
static int b_clear(int argc, char **argv);
static int b_ls(int argc, char **argv);
static int b_cd(int argc, char **argv);
static int b_whereami(int argc, char **argv);
static int b_mkdir(int argc, char **argv);
static int b_create(int argc, char **argv);
static int b_touch(int argc, char **argv);
static int b_del(int argc, char **argv);
static int b_copy(int argc, char **argv);
static int b_mv(int argc, char **argv);
static int b_sleep(int argc, char **argv);
static int b_run(int argc, char **argv);
static int b_read(int argc, char **argv);
static int b_head(int argc, char **argv);
static int b_tail(int argc, char **argv);
static int b_wc(int argc, char **argv);
static int b_write(int argc, char **argv);
static int b_append(int argc, char **argv);
static int b_info(int argc, char **argv);
static int b_exists(int argc, char **argv);
static int b_reboot(int argc, char **argv);
static int b_shutdown(int argc, char **argv);
static int b_beep(int argc, char **argv);
static int b_uptime(int argc, char **argv);
static int b_time(int argc, char **argv);
static int b_rand(int argc, char **argv);
static int b_history(int argc, char **argv);
static int b_exit(int argc, char **argv);

static const builtin_cmd_t builtins[] = {
    {"help",     NULL,     "help",                 b_help},
    {"print",    "echo",   "print|echo [args...]", b_print},
    {"clear",    NULL,     "clear",                b_clear},
    {"ls",       "dir",    "ls|dir [path]",        b_ls},
    {"cd",       NULL,     "cd <path>",            b_cd},
    {"whereami", NULL,     "whereami",             b_whereami},
    {"mkdir",    NULL,     "mkdir <path>",         b_mkdir},
    {"create",   NULL,     "create <filename>",    b_create},
    {"touch",    NULL,     "touch <filename>",     b_touch},
    {"del",      "rm",     "del|rm <path>",        b_del},
    {"copy",     "cp",     "copy|cp <src> <dst>",  b_copy},
    {"mv",       NULL,     "mv <src> <dst>",       b_mv},
    {"read",     "cat",    "read|cat <filename>",  b_read},
    {"head",     NULL,     "head [-n N] <file>",   b_head},
    {"tail",     NULL,     "tail [-n N] <file>",   b_tail},
    {"wc",       NULL,     "wc <filename>",        b_wc},
    {"write",    NULL,     "write <file> <text>",  b_write},
    {"append",   NULL,     "append <file> <text>", b_append},
    {"stat",     NULL,     "stat <path>",          b_info},
    {"exists",   NULL,     "exists <path>",        b_exists},
    {"run",      NULL,     "run <path>",           b_run},
    {"sleep",    NULL,     "sleep <ms>",           b_sleep},
    {"beep",     NULL,     "beep <hz> <ms>",       b_beep},
    {"uptime",   NULL,     "uptime",               b_uptime},
    {"time",     NULL,     "time",                 b_time},
    {"rand",     NULL,     "rand [min] [max]",     b_rand},
    {"history",  NULL,     "history",              b_history},
    {"reboot",   NULL,     "reboot",               b_reboot},
    {"shutdown", NULL,     "shutdown",             b_shutdown},
    {"exit",     NULL,     "exit",                 b_exit},
    {NULL,       NULL,     NULL,                   NULL}
};

static int b_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("Built-in commands:\n");
    for (int i = 0; builtins[i].name != NULL; i++) {
        printf("  %s\n", builtins[i].usage);
    }
    printf("External programs in %s are also executed automatically.\n", PROGRAMS_DIR);
    return 0;
}

static int b_print(int argc, char **argv) {
    if (argc == 1) {
        printf("\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        print_str(argv[i]);
        if (i + 1 < argc) {
            print_str(" ");
        }
    }
    printf("\n");
    return 0;
}

static int b_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    clear();
    return 0;
}

static int b_ls(int argc, char **argv) {
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

static int b_cd(int argc, char **argv) {
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

static int b_whereami(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char cwd[CMD_BUF_SIZE];
    memset(cwd, 0, sizeof(cwd));
    if (fs_get_dir(cwd, sizeof(cwd)) != 0) {
        printf("Failed to get current directory.\n");
        return -1;
    }
    printf("%s\n", cwd);
    return 0;
}

static int b_mkdir(int argc, char **argv) {
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

static int b_create(int argc, char **argv) {
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

static int b_touch(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: touch <filename>\n");
        return -1;
    }
    if (!fs_exists(argv[1]) && fs_create(argv[1]) != 0) {
        printf("Failed to touch file.\n");
        return -1;
    }
    return 0;
}

static int b_del(int argc, char **argv) {
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

static int b_copy(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <src> <dst>\n", argv[0]);
        return -1;
    }
    return copy_file_data(argv[1], argv[2]);
}

static int b_mv(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: mv <src> <dst>\n");
        return -1;
    }
    if (strcmp(argv[1], argv[2]) == 0) return 0;
    if (copy_file_data(argv[1], argv[2]) != 0) {
        return -1;
    }
    if (fs_delete(argv[1]) != 0) {
        printf("Copied to %s, but failed to remove %s.\n", argv[2], argv[1]);
        return -1;
    }
    return 0;
}

static int b_sleep(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: sleep <ms>\n");
        return -1;
    }
    wait(parse_u64(argv[1]));
    return 0;
}

static int b_run(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: run <path>\n");
        return -1;
    }
    if (fs_size(argv[1]) < 0) {
        printf("Program/file doesn't exist.\n");
        return -1;
    }
    return run_program_file(argv[1], 0, NULL);
}

static int b_read(int argc, char **argv) {
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
    if (bytes < 0) {
        printf("Failed to read file.\n");
        return -1;
    }
    if (bytes >= (int64_t)sizeof(buffer)) {
        bytes = (int64_t)sizeof(buffer) - 1;
    }
    buffer[bytes] = '\0';
    printf("%s\n", buffer);
    return 0;
}

static int b_head(int argc, char **argv) {
    int max_lines = 10;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_lines = parse_int_default(argv[++i], 10);
        } else if (!file) {
            file = argv[i];
        }
    }

    if (!file) {
        printf("Usage: head [-n N] <file>\n");
        return -1;
    }
    if (!fs_exists(file)) {
        printf("File doesn't exist.\n");
        return -1;
    }
    if (max_lines <= 0) return 0;

    static char buffer[READ_BUF_SIZE];
    int64_t n = fs_read_all(file, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        printf("Failed to read file.\n");
        return -1;
    }
    buffer[n] = '\0';

    int printed = 0;
    for (int64_t i = 0; i < n && printed < max_lines; i++) {
        putchar(buffer[i]);
        if (buffer[i] == '\n') {
            printed++;
        }
    }
    if (n > 0 && buffer[n - 1] != '\n') {
        putchar('\n');
    }
    return 0;
}

static int b_tail(int argc, char **argv) {
    int want = 10;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            want = parse_int_default(argv[++i], 10);
        } else if (!file) {
            file = argv[i];
        }
    }

    if (!file) {
        printf("Usage: tail [-n N] <file>\n");
        return -1;
    }
    if (!fs_exists(file)) {
        printf("File doesn't exist.\n");
        return -1;
    }
    if (want <= 0) return 0;

    static char buffer[READ_BUF_SIZE];
    int64_t n = fs_read_all(file, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        printf("Failed to read file.\n");
        return -1;
    }
    buffer[n] = '\0';

    int line_starts[READ_BUF_SIZE / 2];
    int total_lines = 0;
    if (n > 0 && total_lines < (int)(sizeof(line_starts) / sizeof(line_starts[0]))) {
        line_starts[total_lines++] = 0;
    }
    for (int64_t i = 0; i < n; i++) {
        if (buffer[i] == '\n' && i + 1 < n) {
            if (total_lines < (int)(sizeof(line_starts) / sizeof(line_starts[0]))) {
                line_starts[total_lines++] = (int)(i + 1);
            }
        }
    }

    int keep = (want > TAIL_MAX_LINES) ? TAIL_MAX_LINES : want;
    int start_line = (total_lines > keep) ? (total_lines - keep) : 0;
    if (total_lines > 0) {
        printf("%s", &buffer[line_starts[start_line]]);
        if (buffer[n - 1] != '\n') {
            putchar('\n');
        }
    }
    return 0;
}

static int b_wc(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: wc <filename>\n");
        return -1;
    }
    if (!fs_exists(argv[1])) {
        printf("File doesn't exist.\n");
        return -1;
    }

    static char buffer[READ_BUF_SIZE];
    int64_t n = fs_read_all(argv[1], buffer, sizeof(buffer) - 1);
    if (n < 0) {
        printf("Failed to read file.\n");
        return -1;
    }

    uint64_t lines = 0, words = 0;
    bool in_word = false;
    for (int64_t i = 0; i < n; i++) {
        char c = buffer[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words++;
        }
    }

    char lbuf[NUM_BUF_SIZE], wbuf[NUM_BUF_SIZE], bbuf[NUM_BUF_SIZE];
    fmt_u64(lines, lbuf, sizeof(lbuf));
    fmt_u64(words, wbuf, sizeof(wbuf));
    fmt_u64((uint64_t)n, bbuf, sizeof(bbuf));
    printf("%s lines  %s words  %s bytes  %s\n", lbuf, wbuf, bbuf, argv[1]);
    return 0;
}

static int b_write(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: write <filename> <text>\n");
        return -1;
    }
    if (!fs_exists(argv[1]) && fs_create(argv[1]) != 0) {
        printf("Failed to create file.\n");
        return -1;
    }
    size_t len = strlen(argv[2]);
    if (fs_write_all(argv[1], argv[2], (uint64_t)len) != 0) {
        printf("Failed to write file.\n");
        return -1;
    }
    return 0;
}

static int b_append(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: append <filename> <text>\n");
        return -1;
    }
    if (!fs_exists(argv[1]) && fs_create(argv[1]) != 0) {
        printf("Failed to create file.\n");
        return -1;
    }
    size_t len = strlen(argv[2]);
    if (fs_append(argv[1], argv[2], (uint64_t)len) != 0) {
        printf("Failed to append to file.\n");
        return -1;
    }
    return 0;
}

static int b_info(int argc, char **argv) {
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

static int b_exists(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: exists <path>\n");
        return -1;
    }
    printf("%s\n", fs_exists(argv[1]) ? "Exists." : "Does not exist.");
    return 0;
}

static int b_reboot(int argc, char **argv) {
    (void)argc;
    (void)argv;
    reboot();
    return 0;
}

static int b_shutdown(int argc, char **argv) {
    (void)argc;
    (void)argv;
    shutdown();
    return 0;
}

static int b_beep(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: beep <hz> <ms>\n");
        return -1;
    }
    beep(parse_u64(argv[1]), parse_u64(argv[2]));
    return 0;
}

static int b_uptime(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char buf[NUM_BUF_SIZE];
    fmt_u64(uptime(), buf, sizeof(buf));
    printf("uptime: %s\n", buf);
    return 0;
}

static int b_time(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char tbuf[128];
    memset(tbuf, 0, sizeof(tbuf));
    if (get_time_str(tbuf, sizeof(tbuf)) < 0) {
        printf("Failed to get time.\n");
        return -1;
    }
    printf("%s\n", tbuf);
    return 0;
}

static int b_rand(int argc, char **argv) {
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
        if (min_v > max_v) {
            uint32_t tmp = min_v;
            min_v = max_v;
            max_v = tmp;
        }
        val = rand_range(min_v, max_v);
    }
    char rbuf[NUM_BUF_SIZE];
    fmt_u64(val, rbuf, sizeof(rbuf));
    printf("%s\n", rbuf);
    return 0;
}

static int b_history(int argc, char **argv) {
    (void)argc;
    (void)argv;
    history_print();
    return 0;
}

static int b_exit(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("exiting\n");
    exit();
    return 0;
}

static builtin_fn_t find_builtin(const char *name) {
    if (!name) return NULL;
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(builtins[i].name, name) == 0 ||
            (builtins[i].alias != NULL && strcmp(builtins[i].alias, name) == 0)) {
            return builtins[i].fn;
        }
    }
    return NULL;
}

int execute(int argc, char **argv) {
    if (argc <= 0 || argv == NULL || argv[0] == NULL) {
        return -1;
    }

    builtin_fn_t fn = find_builtin(argv[0]);
    if (fn != NULL) {
        return fn(argc, argv);
    }

    char prog_path[CMD_BUF_SIZE];
    if (has_path_prefix(argv[0])) {
        copy_str_bounded(prog_path, argv[0], sizeof(prog_path));
        if (fs_size(prog_path) >= 0) {
            return run_program_file(prog_path, argc, argv);
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
        memset(command, 0, sizeof(command));
        if (input(command, sizeof(command) - 1) < 0) {
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