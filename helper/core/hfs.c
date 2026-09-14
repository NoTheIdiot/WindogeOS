#include <system.h>
#include <stdint.h>
#include <string.h>
#include <dogeio.h>
#include <basicutil.h>
#include <bool.h>

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

    if (str_strcmp(username, "admin") == 0) {
        return 1;
    }

    const char* p = target_path;
    while (p[0] == '/') {
        p++;
    }

    if (str_strncmp(p, "system", 6) == 0 && (p[6] == '/' || p[6] == '\0')) {
        return 0;
    }

    if (str_strncmp(p, "users/", 6) == 0) {
        const char* folder_start = p + 6;
        char target_folder[64] = {0};
        int i = 0;

        while (folder_start[i] != '\0' && folder_start[i] != '/' && i < 63) {
            target_folder[i] = folder_start[i];
            i++;
        }
        target_folder[i] = '\0';

        if (target_folder[0] != '\0' && str_strcmp(target_folder, username) != 0) {
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
    str_strcpy(filename, name);
    str_strcat(filename, ".account");

    char buffer[256];
    char perm_str[16];
    str_itoa(permission_id, perm_str);

    buffer[0] = '\0';
    str_strcpy(buffer, name);
    str_strcat(buffer, "\n");
    str_strcat(buffer, password);
    str_strcat(buffer, "\n");
    str_strcat(buffer, perm_str);
    str_strcat(buffer, "\n");

    fs_set_auth_override(1);

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
        fs_set_auth_override(0);
        return 0;
    }

    fs_write(filename, buffer);

    fs_chdir("/");
    fs_chdir("/users");

    if (!fs_exists(name)) {
        fs_mkdir(name);
    }

    fs_chdir("/");
    fs_set_auth_override(0);

    return 1;
}

int system_verify_user(const char* name, char* password) {
    if (!name || !password || name[0] == '\0' || password[0] == '\0') {
        return 0;
    }

    char clean_name[64];
    char clean_pass[64];
    str_strcpy(clean_name, name);
    str_strcpy(clean_pass, password);
    clean_string(clean_name);
    clean_string(clean_pass);

    char filename[128];
    str_strcpy(filename, clean_name);
    str_strcat(filename, ".account");

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
                str_strcpy(stored_password, line_buf);
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

    return (str_strcmp(clean_pass, stored_password) == 0) ? 1 : 0;
}