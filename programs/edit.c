#include "../dogec/stdio.h"
#include "../dogec/dogeio.h"
#include "../dogec/string.h"

#define MAX_LINES 24
#define MAX_LEN   80

static char lines[MAX_LINES][MAX_LEN];
static int nlines = 1;
static int cur = 0;
static char fname[32] = "";

static void copy_str(char *dst, const char *src, int cap) {
    if (!dst || cap <= 0) return;
    int i = 0;
    if (src) {
        while (i < cap - 1 && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static int str_len(const char *s) {
    int n = 0;
    if (!s) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static int str_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    int i = 0;
    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static void prompt(const char *p, char *out, int cap) {
    int pos = 0;
    out[0] = '\0';
    printf("\n%s", p);
    while (pos < cap - 1) {
        int k = getkey();
        if (k < 0) {
            wait(10);
            continue;
        }
        if (k == '\r' || k == '\n') break;
        if ((k == 8 || k == 127) && pos > 0) {
            pos--;
            out[pos] = '\0';
            printf("\b \b");
        } else if (k >= 32 && k < 127) {
            out[pos++] = (char)k;
            out[pos] = '\0';
            putchar(k);
        }
    }
    out[pos] = '\0';
    putchar('\n');
}

static void load_file(const char *path) {
    if (!path || path[0] == '\0') return;
    FILE *fp = fopen(path, "r");
    if (!fp) return;

    nlines = 0;
    char b[MAX_LEN];
    while (nlines < MAX_LINES && fgets(b, sizeof(b), fp) != NULL) {
        int l = str_len(b);
        while (l > 0 && (b[l - 1] == '\n' || b[l - 1] == '\r')) {
            b[--l] = '\0';
        }
        copy_str(lines[nlines], b, MAX_LEN);
        nlines++;
    }
    fclose(fp);

    if (nlines == 0) {
        lines[0][0] = '\0';
        nlines = 1;
    }
    copy_str(fname, path, sizeof(fname));
    cur = 0;
}

static void save_file(const char *path) {
    const char *t = (path && path[0] != '\0') ? path : fname;
    if (!t || t[0] == '\0') {
        printf("No filename\n");
        return;
    }
    FILE *fp = fopen(t, "w");
    if (!fp) {
        printf("Write error\n");
        return;
    }
    for (int i = 0; i < nlines; i++) {
        int l = str_len(lines[i]);
        if (l > 0) fwrite(lines[i], 1, (size_t)l, fp);
        fputc('\n', fp);
    }
    fclose(fp);
    copy_str(fname, t, sizeof(fname));
    printf("Saved %s\n", fname);
}

void _start(int argc, char** argv) {
    for (int i = 0; i < MAX_LINES; i++) lines[i][0] = '\0';
    char buf[MAX_LEN];
    if (argc <= 1) {
        char buf[MAX_LEN];
        prompt("File: ", buf, sizeof(buf));
        if (buf[0] != '\0') {
            load_file(buf);
        }
    } else {
        printf(argv[1]);
        load_file(argv[1]);
    }

    while (1) {
        clear();
        printf("[%s] L:%d/%d\n", fname[0] ? fname : "new", cur + 1, nlines);
        for (int i = 0; i < nlines; i++) {
            printf("%c%d: %s\n", (i == cur) ? '>' : ' ', i + 1, lines[i]);
        }
        printf("(j/k e:edit i:ins d:del :w/:q)\n");

        int k = -1;
        while ((k = getkey()) < 0) {
            wait(10);
        }

        if (k == 'k' && cur > 0) {
            cur--;
        } else if (k == 'j' && cur < nlines - 1) {
            cur++;
        } else if (k == 'e') {
            prompt("Edit: ", buf, sizeof(buf));
            copy_str(lines[cur], buf, MAX_LEN);
        } else if (k == 'i' && nlines < MAX_LINES) {
            for (int i = nlines; i > cur; i--) {
                copy_str(lines[i], lines[i - 1], MAX_LEN);
            }
            lines[cur][0] = '\0';
            nlines++;
            prompt("Ins: ", buf, sizeof(buf));
            copy_str(lines[cur], buf, MAX_LEN);
        } else if (k == 'd') {
            if (nlines == 1) {
                lines[0][0] = '\0';
            } else {
                for (int i = cur; i < nlines - 1; i++) {
                    copy_str(lines[i], lines[i + 1], MAX_LEN);
                }
                lines[nlines - 1][0] = '\0';
                nlines--;
                if (cur >= nlines) cur = nlines - 1;
            }
        } else if (k == ':') {
            prompt(":", buf, sizeof(buf));
            if (str_eq(buf, "q")) break;
            if (str_eq(buf, "w")) save_file(0);
            else if (starts_with(buf, "w ")) save_file(buf + 2);
        } else if (k == 'q') {
            exit();
        }
    }

    clear();
    exit();
    return;
}