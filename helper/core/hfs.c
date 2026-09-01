#include <system.h>
#include <stdint.h>
#include <string.h>
#include <dogeio.h>
#include <basicutil.h>

int system_create_user(char* name, char* password) {
    fs_chdir("/system/pass");

    char passfile[128];
    str_strcpy(passfile, name);
    str_strcat(passfile, ".pass");
    fs_create(passfile);
    fs_write(passfile, password);

    fs_chdir("/");
    fs_chdir("/users");
    fs_mkdir(name);

    fs_chdir("/");
    return 1;
}

static int system_user_file_path(const char* name, char* out, size_t out_len) {
    if (!name || name[0] == '\0' || !out || out_len == 0) {
        return 0;
    }

    if (str_strcmp(name, "root") == 0) {
        str_strcpy(out, "root.hash");
        return 1;
    }

    if (str_strcmp(name, "admin") == 0) {
        str_strcpy(out, "admin.hash");
        return 1;
    }

    str_strcpy(out, name);
    str_strcat(out, ".pass");
    return 1;
}

int system_verify_user(const char* name, char* password) {
    if (!name || !password || name[0] == '\0' || password[0] == '\0') {
        return 0;
    }

    fs_chdir("/system/pass");

    char passfile[128];
    if (!system_user_file_path(name, passfile, sizeof(passfile))) {
        fs_chdir("/");
        return 0;
    }

    char stored[256];
    for (int i = 0; i < 256; i++) {
        stored[i] = 0;
    }

    int bytes_read = fs_read(passfile, stored, sizeof(stored) - 1);
    fs_chdir("/");
    if (bytes_read < 0) {
        return 0;
    }

    stored[bytes_read] = '\0';
    return (str_strcmp(password, stored) == 0) ? 1 : 0;
}

