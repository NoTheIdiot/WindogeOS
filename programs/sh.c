#include "../dogec/dogeio.h"
#include "../dogec/stdio.h"
#include "../dogec/string.h"

#define MAX_LINE 256
#define MAX_ARGS 16
#define MAX_FILES 32
#define IO_BUF_SIZE 4096

static size_t k_strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

static int parse_args(char *line, char **argv, int max_args) {
    memset(argv, 0, sizeof(char *) * max_args);
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            *p++ = '\0';
        }
        if (*p == '\0') break;

        argv[argc++] = p;

        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            p++;
        }
    }
    return argc;
}

static void out_write(FILE *out, const char *str, size_t len) {
    if (!out || out == stdout) {
        fwrite(str, 1, len, stdout);
    } else {
        fwrite(str, 1, len, out);
    }
}

static void out_str(FILE *out, const char *str) {
    out_write(out, str, k_strlen(str));
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
    for (int i = 0; i < count; i++) {
        out_str(out, entries[i].name);
        out_str(out, "  ");
    }
    out_str(out, "\n");
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
    memset(buf, 0, sizeof(buf));
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out_write(out, buf, n);
    }
    fclose(fp);
}

static void cmd_cp(const char *src, const char *dst) {
    if (!src || !dst) {
        printf("usage: cp <src> <dst>\n");
        return;
    }
    FILE *in = fopen(src, "r");
    if (!in) {
        printf("cp: cannot open '%s'\n", src);
        return;
    }
    FILE *out = fopen(dst, "w");
    if (!out) {
        printf("cp: cannot create '%s'\n", dst);
        fclose(in);
        return;
    }

    static char buf[IO_BUF_SIZE];
    memset(buf, 0, sizeof(buf));
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
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
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("sh: cannot open script '%s'\n", filename);
        return;
    }

    static char script_buf[IO_BUF_SIZE];
    memset(script_buf, 0, sizeof(script_buf));
    fread(script_buf, 1, sizeof(script_buf) - 1, fp);
    fclose(fp);

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
}

static void eval_line(char *line) {
    char *argv[MAX_ARGS];
    int argc = parse_args(line, argv, MAX_ARGS);
    if (argc == 0 || argv[0][0] == '#') return;

    FILE *out = stdout;
    const char *redir_file = NULL;
    const char *redir_mode = NULL;

    for (int i = 0; i < argc; i++) {
        if (k_strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            redir_file = argv[i + 1];
            redir_mode = "w";
            memset(&argv[i], 0, sizeof(char *) * (argc - i));
            argc = i;
            break;
        } else if (k_strcmp(argv[i], ">>") == 0 && i + 1 < argc) {
            redir_file = argv[i + 1];
            redir_mode = "a";
            memset(&argv[i], 0, sizeof(char *) * (argc - i));
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

    if (k_strcmp(argv[0], "exit") == 0) {
        if (out != stdout) fclose(out);
        exit();
    } else if (k_strcmp(argv[0], "clear") == 0) {
        clear();
    } else if (k_strcmp(argv[0], "cd") == 0) {
        const char *dir = (argc > 1) ? argv[1] : "/";
        if (fs_chdir(dir) != 0) printf("cd: failed to enter '%s'\n", dir);
    } else if (k_strcmp(argv[0], "pwd") == 0) {
        char cwd[FILENAME_MAX];
        memset(cwd, 0, sizeof(cwd));
        if (fs_get_dir(cwd, sizeof(cwd)) == 0) {
            out_str(out, cwd);
            out_str(out, "\n");
        }
    } else if (k_strcmp(argv[0], "ls") == 0) {
        cmd_ls(argc > 1 ? argv[1] : ".", out);
    } else if (k_strcmp(argv[0], "cat") == 0) {
        cmd_cat(argc > 1 ? argv[1] : NULL, out);
    } else if (k_strcmp(argv[0], "echo") == 0) {
        cmd_echo(argc, argv, out);
    } else if (k_strcmp(argv[0], "cp") == 0) {
        cmd_cp(argc > 1 ? argv[1] : NULL, argc > 2 ? argv[2] : NULL);
    } else if (k_strcmp(argv[0], "touch") == 0) {
        if (argc < 2) printf("usage: touch <file>\n");
        else if (fs_create(argv[1]) != 0) printf("touch: failed\n");
    } else if (k_strcmp(argv[0], "rm") == 0) {
        if (argc < 2) printf("usage: rm <file>\n");
        else if (fs_delete(argv[1]) != 0) printf("rm: failed\n");
    } else if (k_strcmp(argv[0], "sh") == 0) {
        if (argc < 2) printf("usage: sh <script>\n");
        else run_script(argv[1]);
    } else if (k_strcmp(argv[0], "uptime") == 0) {
        char num_buf[64];
        memset(num_buf, 0, sizeof(num_buf));
        snprintf(num_buf, sizeof(num_buf), "Uptime: %llu\n", (unsigned long long)uptime());
        out_str(out, num_buf);
    } else if (k_strcmp(argv[0], "help") == 0) {
        out_str(out, "Commands: ls, cd, pwd, cat, echo, cp, touch, rm, sh, clear, uptime, exit\n");
        out_str(out, "Supports '>' and '>>' output redirection.\n");
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
