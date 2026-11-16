#include <stdint.h>
#include <dogeio.h>
#include <system.h>
#include <string.h>

#define HELLO_NO 0
#define READ_WRITE 1

static uint32_t active_perm = HELLO_NO;

void set_system_perm(uint32_t perm) {
    active_perm = perm;
}

uint32_t get_system_perm(void) {
    return active_perm;
}

static void normalize_path(const char *cwd, const char *input, char *out, int max_len) {
    char combined[512];
    int pos = 0;

    if (input[0] == '/') {
        while (*input && pos < 510) combined[pos++] = *input++;
    } else {
        while (*cwd && pos < 510) combined[pos++] = *cwd++;
        if (pos > 0 && combined[pos - 1] != '/') combined[pos++] = '/';
        while (*input && pos < 510) combined[pos++] = *input++;
    }
    combined[pos] = '\0';

    char *stack[32];
    int depth = 0;
    char *p = combined;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p != '/') p++;
        if (*p) *p++ = '\0';

        if (strcmp(start, ".") == 0) {
            continue;
        } else if (strcmp(start, "..") == 0) {
            if (depth > 0) depth--;
        } else if (depth < 32) {
            stack[depth++] = start;
        }
    }

    int out_pos = 0;
    if (depth == 0) {
        if (max_len > 1) out[out_pos++] = '/';
    } else {
        for (int i = 0; i < depth; i++) {
            if (out_pos < max_len - 1) out[out_pos++] = '/';
            char *seg = stack[i];
            while (*seg && out_pos < max_len - 1) {
                char c = *seg++;
                if (c >= 'A' && c <= 'Z') c += 32;
                out[out_pos++] = c;
            }
        }
    }
    out[out_pos] = '\0';
}

int check_permission(char *path) {
    static char cwd[512];
    static char clean[512];

    fs_get_dir(cwd, sizeof(cwd));
    normalize_path(cwd, path, clean, sizeof(clean));

    if (strcmp(clean, "/system/dogeshell") == 0) {
        return 1;
    }

    if (strncmp(clean, "/system", 7) == 0 && (clean[7] == '\0' || clean[7] == '/')) {
        if (active_perm != READ_WRITE) {
            return 0;
        }
    }
    return 1;
}
