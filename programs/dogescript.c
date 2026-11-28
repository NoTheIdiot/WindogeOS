#include "../dogec/stdio.h"
#include "../dogec/dogeio.h"
#include "../dogec/string.h"

#define MAX_SCRIPT_SIZE 32768
#define MAX_LINE_SIZE   256

static char line[MAX_LINE_SIZE];
static char script[MAX_SCRIPT_SIZE];

static char *skip_ws(char *s) {
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

static int starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix)
            return 0;

        str++;
        prefix++;
    }

    return 1;
}

static uint64_t parse_u64(const char *s) {
    uint64_t v = 0;

    while (*s >= '0' && *s <= '9') {
        v *= 10;
        v += (uint64_t)(*s - '0');
        s++;
    }

    return v;
}

static void strip_quotes(char *s) {

    size_t len = strlen(s);

    if (len < 2)
        return;

    if (s[0] != '"')
        return;

    if (s[len - 1] != '"')
        return;

    for (size_t i = 1; i < len - 1; i++)
        s[i - 1] = s[i];

    s[len - 2] = 0;
}

static void cmd_print(char *args) {

    strip_quotes(args);

    printf("%s", args);
}

static void cmd_println(char *args) {

    strip_quotes(args);

    printf("%s\n", args);
}

static void cmd_wait(char *args) {

    wait(parse_u64(args));
}

static void cmd_beep(char *args) {

    uint64_t hz;
    uint64_t ms;

    hz = parse_u64(args);

    while (*args && *args != ' ')
        args++;

    while (*args == ' ')
        args++;

    ms = parse_u64(args);

    beep(hz, ms);
}

static void cmd_run(char *args) {

    strip_quotes(args);

    int64_t size = fs_size(args);

    if (size <= 0) {
        printf("run: file not found\n");
        return;
    }

    run(args, (size_t)size, 0, NULL);
}

static void execute_line(char *src) {

    src = skip_ws(src);

    if (*src == 0)
        return;

    if (*src == '#')
        return;

    if (starts_with(src, "print ")) {
        cmd_print(src + 6);
        return;
    }

    if (starts_with(src, "println ")) {
        cmd_println(src + 8);
        return;
    }

    if (starts_with(src, "wait ")) {
        cmd_wait(src + 5);
        return;
    }

    if (starts_with(src, "beep ")) {
        cmd_beep(src + 5);
        return;
    }

    if (starts_with(src, "run ")) {
        cmd_run(src + 4);
        return;
    }

    if (strcmp(src, "clear") == 0) {
        clear();
        return;
    }

    if (strcmp(src, "reboot") == 0) {
        reboot();
        return;
    }

    if (strcmp(src, "shutdown") == 0) {
        shutdown();
        return;
    }

    if (strcmp(src, "exit") == 0) {
        proc_exit_now();
        return;
    }

    printf("DogeScript: unknown command: %s\n", src);
}

static void execute_script(const char *src) {

    uint64_t pos = 0;

    while (src[pos]) {

        uint64_t out = 0;

        while (src[pos] &&
               src[pos] != '\n' &&
               out < (MAX_LINE_SIZE - 1)) {

            line[out++] = src[pos++];
        }

        line[out] = 0;

        if (src[pos] == '\n')
            pos++;

        execute_line(line);
    }
}

void _start(int argc, char **argv) {

    if (argc < 2) {
        printf("Usage: dogescript <file.doge>\n");
        proc_exit_now();
        return;
    }

    FILE *f = fopen(argv[1], "r");

    if (!f) {
        printf("Cannot open: %s\n", argv[1]);
        proc_exit_now();
        return;
    }

    size_t bytes = fread(
        script,
        1,
        MAX_SCRIPT_SIZE - 1,
        f
    );

    fclose(f);

    script[bytes] = 0;

    execute_script(script);

    proc_exit_now();
}