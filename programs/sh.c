#include "../dogec/dogeio.h"
#include "../dogec/stdio.h"
#include "../dogec/string.h"

#define MAX_LINE         256
#define MAX_ARGS         16
#define MAX_FILES        32
#define IO_BUF_SIZE      4096
#define TAIL_MAX_LINES   32
#define MAX_SCRIPT_DEPTH 8

typedef int (*cmd_fn_t)(int argc, char **argv, FILE *out);

typedef struct {
    const char *name;
    cmd_fn_t fn;
    const char *help;
} command_t;

static int g_script_depth = 0;

static int cmd_ls(int argc, char **argv, FILE *out);
static int cmd_cat(int argc, char **argv, FILE *out);
static int cmd_cp(int argc, char **argv, FILE *out);
static int cmd_mv(int argc, char **argv, FILE *out);
static int cmd_pwd(int argc, char **argv, FILE *out);
static int cmd_cd(int argc, char **argv, FILE *out);
static int cmd_mkdir(int argc, char **argv, FILE *out);
static int cmd_touch(int argc, char **argv, FILE *out);
static int cmd_rm(int argc, char **argv, FILE *out);
static int cmd_head(int argc, char **argv, FILE *out);
static int cmd_tail(int argc, char **argv, FILE *out);
static int cmd_wc(int argc, char **argv, FILE *out);
static int cmd_echo(int argc, char **argv, FILE *out);
static int cmd_sh(int argc, char **argv, FILE *out);
static int cmd_clear(int argc, char **argv, FILE *out);
static int cmd_uptime(int argc, char **argv, FILE *out);
static int cmd_help(int argc, char **argv, FILE *out);
static int cmd_exit(int argc, char **argv, FILE *out);
static int cmd_reboot(int argc, char **argv, FILE *out);
static int cmd_shutdown(int argc, char **argv, FILE *out);

static const command_t builtins[] = {
    {"ls",       cmd_ls,       "ls [dir] - List directory entries"},
    {"cd",       cmd_cd,       "cd [dir] - Change current directory"},
    {"pwd",      cmd_pwd,      "pwd - Print current directory"},
    {"mkdir",    cmd_mkdir,    "mkdir <dir> - Create directory"},
    {"cat",      cmd_cat,      "cat <file> - Print file contents"},
    {"head",     cmd_head,     "head [-n lines] <file> - Print first lines"},
    {"tail",     cmd_tail,     "tail [-n lines] <file> - Print last lines"},
    {"wc",       cmd_wc,       "wc <file> - Count lines, words, and bytes"},
    {"echo",     cmd_echo,     "echo [args...] - Print arguments"},
    {"cp",       cmd_cp,       "cp <src> <dst> - Copy file"},
    {"mv",       cmd_mv,       "mv <src> <dst> - Move/rename file"},
    {"touch",    cmd_touch,    "touch <file> - Create empty file"},
    {"rm",       cmd_rm,       "rm <file> - Delete file"},
    {"sh",       cmd_sh,       "sh <script> - Run script file"},
    {"clear",    cmd_clear,    "clear - Clear screen"},
    {"uptime",   cmd_uptime,   "uptime - Show system uptime"},
    {"help",     cmd_help,     "help - Show available commands"},
    {"exit",     cmd_exit,     "exit - Exit shell"},
    {"reboot",   cmd_reboot,   "reboot - Reboot system"},
    {"shutdown", cmd_shutdown, "shutdown - Shut down system"},
    {NULL, NULL, NULL}
};

static void out_write(FILE *out, const char *str, size_t len) {
    if (!str || len == 0) return;
    if (!out) out = stdout;
    fwrite(str, 1, len, out);
}

static void out_str(FILE *out, const char *str) {
    if (!str) return;
    out_write(out, str, strlen(str));
}

static char *trim_leading_ws(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }
    return s;
}

static int parse_args(char *line, char **argv, int max_args) {
    memset(argv, 0, sizeof(char *) * (size_t)max_args);
    if (!line || !argv || max_args <= 1) return 0;

    int argc = 0;
    char *src = line;

    while (*src && argc < max_args - 1) {
        src = trim_leading_ws(src);
        if (*src == '\0') break;

        char *dst = src;
        argv[argc] = dst;
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
            printf("sh: syntax error: unclosed quote\n");
            return -1;
        }

        *dst = '\0';
        argc++;
    }

    argv[argc] = NULL;
    return argc;
}

static int parse_int_or_default(const char *s, int fallback) {
    if (!s || !*s) return fallback;
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

static int cmd_ls(int argc, char **argv, FILE *out) {
    fs_t entries[MAX_FILES];
    memset(entries, 0, sizeof(entries));

    const char *target = (argc > 1) ? argv[1] : ".";
    int count = fs_list(target, entries, MAX_FILES);
    if (count < 0) {
        printf("ls: failed to list '%s'\n", target);
        return -1;
    }

    char line_buf[160];
    for (int i = 0; i < count; i++) {
        memset(line_buf, 0, sizeof(line_buf));
        if (entries[i].is_dir) {
            snprintf(line_buf, sizeof(line_buf), "<DIR>  %s\n", entries[i].name);
        } else {
            snprintf(line_buf, sizeof(line_buf), "%llu B  %s\n",
                     (unsigned long long)entries[i].size, entries[i].name);
        }
        out_str(out, line_buf);
    }
    return 0;
}

static int cmd_cat(int argc, char **argv, FILE *out) {
    if (argc < 2) {
        printf("usage: cat <file>\n");
        return -1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        printf("cat: cannot open '%s'\n", argv[1]);
        return -1;
    }

    char buf[IO_BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out_write(out, buf, n);
    }
    fclose(fp);
    return 0;
}

static int copy_file_contents(const char *src, const char *dst) {
    FILE *in = fopen(src, "r");
    if (!in) {
        printf("cp: cannot open '%s'\n", src);
        return -1;
    }

    FILE *out = fopen(dst, "w");
    if (!out) {
        printf("cp: cannot create '%s'\n", dst);
        fclose(in);
        return -1;
    }

    char buf[IO_BUF_SIZE];
    size_t n;
    int failed = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            failed = 1;
            break;
        }
    }

    fclose(in);
    fclose(out);

    if (failed) {
        fs_delete(dst);
        return -1;
    }
    return 0;
}

static int cmd_cp(int argc, char **argv, FILE *out) {
    (void)out;
    if (argc < 3) {
        printf("usage: cp <src> <dst>\n");
        return -1;
    }
    if (strcmp(argv[1], argv[2]) == 0) {
        printf("cp: '%s' and '%s' are the same file\n", argv[1], argv[2]);
        return -1;
    }
    if (copy_file_contents(argv[1], argv[2]) != 0) {
        printf("cp: failed copying '%s' to '%s'\n", argv[1], argv[2]);
        return -1;
    }
    return 0;
}

static int cmd_mv(int argc, char **argv, FILE *out) {
    (void)out;
    if (argc < 3) {
        printf("usage: mv <src> <dst>\n");
        return -1;
    }
    if (strcmp(argv[1], argv[2]) == 0) return 0;

    if (copy_file_contents(argv[1], argv[2]) != 0) {
        printf("mv: failed moving '%s' to '%s'\n", argv[1], argv[2]);
        return -1;
    }
    if (fs_delete(argv[1]) != 0) {
        printf("mv: copied to '%s', but failed to remove '%s'\n", argv[2], argv[1]);
        return -1;
    }
    return 0;
}

static int cmd_cd(int argc, char **argv, FILE *out) {
    (void)out;
    const char *dir = (argc > 1) ? argv[1] : "/";
    if (fs_chdir(dir) != 0) {
        printf("cd: failed to enter '%s'\n", dir);
        return -1;
    }
    return 0;
}

static int cmd_pwd(int argc, char **argv, FILE *out) {
    (void)argc;
    (void)argv;
    char cwd[FILENAME_MAX];
    memset(cwd, 0, sizeof(cwd));
    if (fs_get_dir(cwd, sizeof(cwd)) != 0) {
        printf("pwd: failed to get current directory\n");
        return -1;
    }
    out_str(out, cwd);
    out_str(out, "\n");
    return 0;
}

static int cmd_mkdir(int argc, char **argv, FILE *out) {
    (void)out;
    if (argc < 2) {
        printf("usage: mkdir <dir>\n");
        return -1;
    }
    if (fs_mkdir(argv[1]) != 0) {
        printf("mkdir: failed '%s'\n", argv[1]);
        return -1;
    }
    return 0;
}

static int cmd_touch(int argc, char **argv, FILE *out) {
    (void)out;
    if (argc < 2) {
        printf("usage: touch <file>\n");
        return -1;
    }
    if (fs_create(argv[1]) != 0) {
        printf("touch: failed to create '%s'\n", argv[1]);
        return -1;
    }
    return 0;
}

static int cmd_rm(int argc, char **argv, FILE *out) {
    (void)out;
    if (argc < 2) {
        printf("usage: rm <file>\n");
        return -1;
    }
    if (fs_delete(argv[1]) != 0) {
        printf("rm: failed to remove '%s'\n", argv[1]);
        return -1;
    }
    return 0;
}

static int cmd_head(int argc, char **argv, FILE *out) {
    int lines = 10;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            lines = parse_int_or_default(argv[++i], 10);
        } else if (!file) {
            file = argv[i];
        }
    }

    if (!file) {
        printf("usage: head [-n lines] <file>\n");
        return -1;
    }
    if (lines <= 0) return 0;

    FILE *fp = fopen(file, "r");
    if (!fp) {
        printf("head: cannot open '%s'\n", file);
        return -1;
    }

    char line_buf[MAX_LINE];
    int printed = 0;
    while (printed < lines && fgets(line_buf, sizeof(line_buf), fp) != NULL) {
        out_str(out, line_buf);
        size_t len = strlen(line_buf);
        if (len > 0 && line_buf[len - 1] == '\n') {
            printed++;
        }
    }
    fclose(fp);
    return 0;
}

static int cmd_tail(int argc, char **argv, FILE *out) {
    int lines = 10;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            lines = parse_int_or_default(argv[++i], 10);
        } else if (!file) {
            file = argv[i];
        }
    }

    if (!file) {
        printf("usage: tail [-n lines] <file>\n");
        return -1;
    }
    if (lines <= 0) return 0;

    FILE *fp = fopen(file, "r");
    if (!fp) {
        printf("tail: cannot open '%s'\n", file);
        return -1;
    }

    char ring[TAIL_MAX_LINES][MAX_LINE];
    memset(ring, 0, sizeof(ring));

    int line_idx = 0;
    int at_line_start = 1;
    char chunk[MAX_LINE];

    while (fgets(chunk, sizeof(chunk), fp) != NULL) {
        size_t clen = strlen(chunk);
        if (clen == 0) continue;

        int slot = line_idx % TAIL_MAX_LINES;
        if (at_line_start) {
            ring[slot][0] = '\0';
        }

        size_t cur_len = strlen(ring[slot]);
        if (cur_len + 1 < MAX_LINE) {
            size_t copy_len = MAX_LINE - 1 - cur_len;
            if (clen < copy_len) copy_len = clen;
            memcpy(ring[slot] + cur_len, chunk, copy_len);
            ring[slot][cur_len + copy_len] = '\0';
        }

        if (chunk[clen - 1] == '\n') {
            line_idx++;
            at_line_start = 1;
        } else {
            at_line_start = 0;
        }
    }
    fclose(fp);

    int total_lines = at_line_start ? line_idx : (line_idx + 1);
    int keep = (lines > TAIL_MAX_LINES) ? TAIL_MAX_LINES : lines;
    int start = (total_lines > keep) ? (total_lines - keep) : 0;

    for (int i = start; i < total_lines; i++) {
        out_str(out, ring[i % TAIL_MAX_LINES]);
    }
    return 0;
}

static int cmd_wc(int argc, char **argv, FILE *out) {
    if (argc < 2) {
        printf("usage: wc <file>\n");
        return -1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        printf("wc: cannot open '%s'\n", argv[1]);
        return -1;
    }

    char buf[IO_BUF_SIZE];
    size_t n;
    size_t bytes = 0, lines = 0, words = 0;
    int in_word = 0;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        bytes += n;
        for (size_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') lines++;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
    }
    fclose(fp);

    char stat_buf[128];
    snprintf(stat_buf, sizeof(stat_buf), "%llu lines  %llu words  %llu bytes  %s\n",
             (unsigned long long)lines,
             (unsigned long long)words,
             (unsigned long long)bytes,
             argv[1]);
    out_str(out, stat_buf);
    return 0;
}

static int cmd_echo(int argc, char **argv, FILE *out) {
    for (int i = 1; i < argc; i++) {
        out_str(out, argv[i]);
        if (i + 1 < argc) out_str(out, " ");
    }
    out_str(out, "\n");
    return 0;
}

static int cmd_clear(int argc, char **argv, FILE *out) {
    (void)argc;
    (void)argv;
    (void)out;
    clear();
    return 0;
}

static int cmd_uptime(int argc, char **argv, FILE *out) {
    (void)argc;
    (void)argv;
    char num_buf[64];
    memset(num_buf, 0, sizeof(num_buf));
    snprintf(num_buf, sizeof(num_buf), "Uptime: %llu\n", (unsigned long long)uptime());
    out_str(out, num_buf);
    return 0;
}

static int cmd_help(int argc, char **argv, FILE *out) {
    (void)argc;
    (void)argv;
    out_str(out, "Available commands:\n");
    for (int i = 0; builtins[i].name != NULL; i++) {
        char help_buf[160];
        snprintf(help_buf, sizeof(help_buf), "  %-10s %s\n", builtins[i].name, builtins[i].help);
        out_str(out, help_buf);
    }
    out_str(out, "Supports quoted strings, escape sequences (\\n, \\t), and '>' / '>>' redirection.\n");
    return 0;
}

static int cmd_exit(int argc, char **argv, FILE *out) {
    (void)argc;
    (void)argv;
    if (out && out != stdout) fclose(out);
    exit();
    return 0;
}

static int cmd_reboot(int argc, char **argv, FILE *out) {
    (void)argc;
    (void)argv;
    if (out && out != stdout) fclose(out);
    reboot();
    return 0;
}

static int cmd_shutdown(int argc, char **argv, FILE *out) {
    (void)argc;
    (void)argv;
    if (out && out != stdout) fclose(out);
    shutdown();
    return 0;
}

static int eval_line(char *line);

static int cmd_sh(int argc, char **argv, FILE *out) {
    (void)out;
    if (argc < 2) {
        printf("usage: sh <script>\n");
        return -1;
    }

    const char *filename = argv[1];
    if (g_script_depth >= MAX_SCRIPT_DEPTH) {
        printf("sh: maximum script nesting depth (%d) reached\n", MAX_SCRIPT_DEPTH);
        return -1;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("sh: cannot open script '%s'\n", filename);
        return -1;
    }

    char script_buf[IO_BUF_SIZE];
    memset(script_buf, 0, sizeof(script_buf));
    fread(script_buf, 1, sizeof(script_buf) - 1, fp);
    fclose(fp);

    g_script_depth++;

    char *p = script_buf;
    while (*p) {
        char *line_start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        if (*p) *p++ = '\0';
        while (*p == '\n' || *p == '\r') p++;

        char *trimmed = trim_leading_ws(line_start);
        if (*trimmed != '\0' && *trimmed != '#') {
            eval_line(trimmed);
        }
    }

    g_script_depth--;
    return 0;
}

static const command_t *find_builtin(const char *name) {
    if (!name) return NULL;
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return &builtins[i];
        }
    }
    return NULL;
}

static int eval_line(char *line) {
    char *trimmed = trim_leading_ws(line);
    if (*trimmed == '\0' || *trimmed == '#') return 0;

    char *argv[MAX_ARGS];
    int argc = parse_args(trimmed, argv, MAX_ARGS);
    if (argc <= 0) return argc;

    FILE *out = stdout;
    const char *redir_file = NULL;
    const char *redir_mode = NULL;

    for (int i = 0; i < argc; i++) {
        if ((strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0)) {
            if (i + 1 >= argc) {
                printf("sh: syntax error near unexpected token '%s'\n", argv[i]);
                return -1;
            }
            redir_mode = (strcmp(argv[i], ">>") == 0) ? "a" : "w";
            redir_file = argv[i + 1];
            memset(&argv[i], 0, sizeof(char *) * (size_t)(argc - i));
            argc = i;
            break;
        }
    }

    if (argc == 0) {
        printf("sh: syntax error: missing command before redirection\n");
        return -1;
    }

    if (redir_file) {
        out = fopen(redir_file, redir_mode);
        if (!out) {
            printf("sh: cannot open '%s' for redirection\n", redir_file);
            return -1;
        }
    }

    int status = 0;
    const command_t *cmd = find_builtin(argv[0]);
    if (cmd) {
        status = cmd->fn(argc, argv, out);
    } else {
        int64_t fsize = fs_size(argv[0]);
        if (fsize <= 0) {
            printf("sh: command not found: %s\n", argv[0]);
            status = -1;
        } else if (exec_prog(argv[0], (uint64_t)fsize) < 0) {
            printf("sh: failed to execute '%s'\n", argv[0]);
            status = -1;
        }
    }

    if (out != stdout) {
        fclose(out);
    }
    return status;
}

void _start(void) {
    char line[MAX_LINE];
    char cwd[FILENAME_MAX];

    puts("WindogeOS SH. Type 'help' for commands.");

    while (1) {
        memset(cwd, 0, sizeof(cwd));
        if (fs_get_dir(cwd, sizeof(cwd)) == 0) {
            printf("%s $ ", cwd);
        } else {
            printf("$ ");
        }

        memset(line, 0, sizeof(line));
        if (input(line, sizeof(line) - 1) < 0) {
            puts("\nInput error.");
            continue;
        }
        eval_line(line);
    }
}