#include "../dogec/dogeio.h"
#include "../dogec/stdio.h"
#include "../dogec/string.h"

#define MAX_LINE         256
#define MAX_ARGS         16
#define MAX_FILES        32
#define IO_BUF_SIZE      4096
#define MAX_SCRIPT_DEPTH 8

static int g_script_depth = 0;

static void out_write(FILE *out, const char *str, size_t len) {
    if (!str || len == 0) return;
    if (!out) out = stdout;
    fwrite(str, 1, len, out);
}

static void out_str(FILE *out, const char *str) {
    if (!str) return;
    out_write(out, str, strlen(str));
}

static int parse_args(char *line, char **argv, int max_args) {
    memset(argv, 0, sizeof(char *) * (size_t)max_args);
    if (!line || !argv || max_args <= 1) return 0;

    int argc = 0;
    char *src = line;

    while (*src && argc < max_args - 1) {
        while (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') {
            src++;
        }
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
    int val = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') return fallback;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

static void cmd_ls(const char *path, FILE *out) {
    fs_t entries[MAX_FILES];
    memset(entries, 0, sizeof(entries));

    const char *target = path ? path : ".";
    int count = fs_list(target, entries, MAX_FILES);
    if (count < 0) {
        printf("ls: failed to list '%s'\n", target);
        return;
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
}

static void cmd_cat(const char *filename, FILE *out) {
    if (!filename) {
        printf("usage: cat <file>\n");
        return;
    }
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("cat: cannot open '%s'\n", filename);
        return;
    }

    static char buf[IO_BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out_write(out, buf, n);
    }
    fclose(fp);
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

    static char buf[IO_BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

static void cmd_cp(const char *src, const char *dst) {
    if (!src || !dst) {
        printf("usage: cp <src> <dst>\n");
        return;
    }
    if (copy_file_contents(src, dst) != 0) {
        printf("cp: failed copying '%s' to '%s'\n", src, dst);
    }
}

static void cmd_mv(const char *src, const char *dst) {
    if (!src || !dst) {
        printf("usage: mv <src> <dst>\n");
        return;
    }
    if (strcmp(src, dst) == 0) return;
    if (copy_file_contents(src, dst) != 0) {
        printf("mv: failed moving '%s' to '%s'\n", src, dst);
        return;
    }
    if (fs_delete(src) != 0) {
        printf("mv: copied to '%s', but failed to remove '%s'\n", dst, src);
    }
}

static void cmd_head(int argc, char **argv, FILE *out) {
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
        return;
    }
    if (lines < 0) lines = 0;

    FILE *fp = fopen(file, "r");
    if (!fp) {
        printf("head: cannot open '%s'\n", file);
        return;
    }

    char line_buf[MAX_LINE];
    int printed = 0;
    while (printed < lines && fgets(line_buf, sizeof(line_buf), fp) != NULL) {
        out_str(out, line_buf);
        printed++;
    }
    fclose(fp);
}

static void cmd_tail(int argc, char **argv, FILE *out) {
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
        return;
    }
    if (lines <= 0) return;

    FILE *fp = fopen(file, "r");
    if (!fp) {
        printf("tail: cannot open '%s'\n", file);
        return;
    }

    static char ring[32][MAX_LINE];
    int total = 0;
    while (fgets(ring[total % 32], MAX_LINE, fp) != NULL) {
        total++;
    }
    fclose(fp);

    int keep = (lines > 32) ? 32 : lines;
    int start = (total > keep) ? (total - keep) : 0;
    for (int i = start; i < total; i++) {
        out_str(out, ring[i % 32]);
    }
}

static void cmd_wc(const char *file, FILE *out) {
    if (!file) {
        printf("usage: wc <file>\n");
        return;
    }

    FILE *fp = fopen(file, "r");
    if (!fp) {
        printf("wc: cannot open '%s'\n", file);
        return;
    }

    static char buf[IO_BUF_SIZE];
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
             file);
    out_str(out, stat_buf);
}

static void cmd_echo(int argc, char **argv, FILE *out) {
    for (int i = 1; i < argc; i++) {
        out_str(out, argv[i]);
        if (i + 1 < argc) out_str(out, " ");
    }
    out_str(out, "\n");
}

static void eval_line(char *line);

static void run_script(const char *filename) {
    if (!filename) return;
    if (g_script_depth >= MAX_SCRIPT_DEPTH) {
        printf("sh: maximum script nesting depth (%d) reached\n", MAX_SCRIPT_DEPTH);
        return;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("sh: cannot open script '%s'\n", filename);
        return;
    }

    static char script_buf[IO_BUF_SIZE];
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

        if (line_start[0] != '\0' && line_start[0] != '#') {
            eval_line(line_start);
        }
    }

    g_script_depth--;
}

static void eval_line(char *line) {
    char *argv[MAX_ARGS];
    int argc = parse_args(line, argv, MAX_ARGS);
    if (argc <= 0 || argv[0][0] == '#') return;

    FILE *out = stdout;
    const char *redir_file = NULL;
    const char *redir_mode = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            redir_file = argv[i + 1];
            redir_mode = "w";
            memset(&argv[i], 0, sizeof(char *) * (size_t)(argc - i));
            argc = i;
            break;
        } else if (strcmp(argv[i], ">>") == 0 && i + 1 < argc) {
            redir_file = argv[i + 1];
            redir_mode = "a";
            memset(&argv[i], 0, sizeof(char *) * (size_t)(argc - i));
            argc = i;
            break;
        }
    }

    if (argc == 0) return;

    if (redir_file) {
        out = fopen(redir_file, redir_mode);
        if (!out) {
            printf("sh: cannot open '%s' for redirection\n", redir_file);
            return;
        }
    }

    if (strcmp(argv[0], "exit") == 0) {
        if (out != stdout) fclose(out);
        exit();
    } else if (strcmp(argv[0], "clear") == 0) {
        clear();
    } else if (strcmp(argv[0], "cd") == 0) {
        const char *dir = (argc > 1) ? argv[1] : "/";
        if (fs_chdir(dir) != 0) printf("cd: failed to enter '%s'\n", dir);
    } else if (strcmp(argv[0], "pwd") == 0) {
        char cwd[FILENAME_MAX];
        memset(cwd, 0, sizeof(cwd));
        if (fs_get_dir(cwd, sizeof(cwd)) == 0) {
            out_str(out, cwd);
            out_str(out, "\n");
        }
    } else if (strcmp(argv[0], "mkdir") == 0) {
        if (argc < 2) printf("usage: mkdir <dir>\n");
        else if (fs_mkdir(argv[1]) != 0) printf("mkdir: failed '%s'\n", argv[1]);
    } else if (strcmp(argv[0], "ls") == 0) {
        cmd_ls(argc > 1 ? argv[1] : ".", out);
    } else if (strcmp(argv[0], "cat") == 0) {
        cmd_cat(argc > 1 ? argv[1] : NULL, out);
    } else if (strcmp(argv[0], "head") == 0) {
        cmd_head(argc, argv, out);
    } else if (strcmp(argv[0], "tail") == 0) {
        cmd_tail(argc, argv, out);
    } else if (strcmp(argv[0], "wc") == 0) {
        cmd_wc(argc > 1 ? argv[1] : NULL, out);
    } else if (strcmp(argv[0], "echo") == 0) {
        cmd_echo(argc, argv, out);
    } else if (strcmp(argv[0], "cp") == 0) {
        cmd_cp(argc > 1 ? argv[1] : NULL, argc > 2 ? argv[2] : NULL);
    } else if (strcmp(argv[0], "mv") == 0) {
        cmd_mv(argc > 1 ? argv[1] : NULL, argc > 2 ? argv[2] : NULL);
    } else if (strcmp(argv[0], "touch") == 0) {
        if (argc < 2) printf("usage: touch <file>\n");
        else if (fs_create(argv[1]) != 0) printf("touch: failed\n");
    } else if (strcmp(argv[0], "rm") == 0) {
        if (argc < 2) printf("usage: rm <file>\n");
        else if (fs_delete(argv[1]) != 0) printf("rm: failed\n");
    } else if (strcmp(argv[0], "sh") == 0) {
        if (argc < 2) printf("usage: sh <script>\n");
        else run_script(argv[1]);
    } else if (strcmp(argv[0], "uptime") == 0) {
        char num_buf[64];
        memset(num_buf, 0, sizeof(num_buf));
        snprintf(num_buf, sizeof(num_buf), "Uptime: %llu\n", (unsigned long long)uptime());
        out_str(out, num_buf);
    } else if (strcmp(argv[0], "help") == 0) {
        out_str(out, "Commands: ls, cd, pwd, mkdir, cat, head, tail, wc, echo, cp, mv, touch, rm, sh, clear, uptime, exit\n");
        out_str(out, "Supports quoted strings and '>' / '>>' output redirection.\n");
    } else if (strcmp(argv[0], "reboot") == 0) {
        reboot();
    } else if (strcmp(argv[0], "shutdown") == 0) {
        shutdown();
    } else {
        int64_t fsize = fs_size(argv[0]);
        if (fsize <= 0) {
            printf("sh: command not found: %s\n", argv[0]);
        } else if (exec_prog(argv[0], (uint64_t)fsize) < 0) {
            printf("sh: failed to execute '%s'\n", argv[0]);
        }
    }

    if (out != stdout) {
        fclose(out);
    }
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