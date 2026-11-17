#include <stdint.h>
#include <dogeio.h>
#include <system.h>
#include <string.h>

static uint32_t active_perm = HELL_NO;

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

void switch_permission(uint32_t permission) {
    if (permission != 0 || permission != 1) {
        return;
    }
    active_perm = permission;
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

static void clean_string(char* str) {
    if (!str) return;
    int len = 0;
    while (str[len] != '\0') len++;
    while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n' || str[len - 1] == ' ' || str[len - 1] == '\t')) {
        str[--len] = '\0';
    }
}

int system_can_access_path(const char* username, const char* target_path) {
    if (!username || !target_path) return 0;

    if (strcmp(username, "admin") == 0) {
        return 1;
    }

    const char* p = target_path;
    while (p[0] == '/') {
        p++;
    }

    if (strncmp(p, "system", 6) == 0 && (p[6] == '/' || p[6] == '\0')) {
        return 0;
    }

    if (strncmp(p, "users/", 6) == 0) {
        const char* folder_start = p + 6;
        char target_folder[64] = {0};
        int i = 0;

        while (folder_start[i] != '\0' && folder_start[i] != '/' && i < 63) {
            target_folder[i] = folder_start[i];
            i++;
        }
        target_folder[i] = '\0';

        if (target_folder[0] != '\0' && strcmp(target_folder, username) != 0) {
            return 0;
        }
    }

    return 1;
}

int system_create_user(char* name, char* password, int permission_id) {
    if (!name || name[0] == '\0' || !password) {
        return 0;
    }

    clean_string(name);
    clean_string(password);

    char filename[128];
    strcpy(filename, name);
    strcat(filename, ".account");

    char buffer[256];
    char perm_str[16];
    itoa(permission_id, perm_str);

    buffer[0] = '\0';
    strcpy(buffer, name);
    strcat(buffer, "\n");
    strcat(buffer, password);
    strcat(buffer, "\n");
    strcat(buffer, perm_str);
    strcat(buffer, "\n");

    switch_permission(READ_WRITE);

    fs_chdir("/");
    fs_chdir("/system");
    if (!fs_exists("accounts")) {
        fs_mkdir("accounts");
    }
    fs_chdir("/system/accounts");

    if (fs_exists(filename)) {
        fs_delete(filename);
    }

    if (fs_create(filename) != 0) {
        fs_chdir("/");
        switch_permission(HELL_NO);
        return 0;
    }

    fs_write(filename, buffer, 256);

    fs_chdir("/");
    fs_chdir("/users");

    if (!fs_exists(name)) {
        fs_mkdir(name);
    }

    fs_chdir("/");
    switch_permission(0);

    return 1;
}

int system_verify_user(const char* name, char* password) {
    if (!name || !password || name[0] == '\0' || password[0] == '\0') {
        return 0;
    }

    char clean_name[64];
    char clean_pass[64];
    strcpy(clean_name, name);
    strcpy(clean_pass, password);
    clean_string(clean_name);
    clean_string(clean_pass);

    char filename[128];
    strcpy(filename, clean_name);
    strcat(filename, ".account");

    fs_chdir("/");
    fs_chdir("/system/accounts");

    if (!fs_exists(filename)) {
        fs_chdir("/");
        return 0;
    }

    char buffer[512];
    for (int i = 0; i < (int)sizeof(buffer); i++) {
        buffer[i] = 0;
    }

    int bytes_read = fs_read(filename, buffer, sizeof(buffer) - 1);
    fs_chdir("/");

    if (bytes_read <= 0) {
        return 0;
    }
    buffer[bytes_read] = '\0';

    char line_buf[128];
    int line = 0;
    int idx = 0;
    char stored_password[128] = {0};

    for (int i = 0; buffer[i] != '\0'; i++) {
        char c = buffer[i];
        if (c == '\r') continue;

        if (c == '\n') {
            line_buf[idx] = '\0';
            if (line == 1) {
                strcpy(stored_password, line_buf);
                break;
            }
            line++;
            idx = 0;
        } else {
            if (idx < (int)sizeof(line_buf) - 1) {
                line_buf[idx++] = c;
            }
        }
    }

    return (strcmp(clean_pass, stored_password) == 0) ? 1 : 0;
}