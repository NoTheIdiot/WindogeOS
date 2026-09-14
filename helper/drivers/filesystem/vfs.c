#include <boot/kernel.h>
#include <dogeio.h>
#include <basicutil.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <bool.h>
#include <system.h>

extern int exfat_resolve_entry(const char *target_name, void *out);

static void fs_sanitize_path(const char *in, char *out, size_t out_size) {
    if (!in || !out || out_size == 0) return;

    while (*in == ' ') in++;

    str_strncpy(out, in, out_size - 1);
    out[out_size - 1] = '\0';

    int len = (int)str_strlen(out);

    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r' || out[len - 1] == ' ')) {
        out[--len] = '\0';
    }

    while (len > 1 && out[len - 1] == '/') {
        out[--len] = '\0';
    }
}

static int fs_split_path(const char *path, char *out_parent, size_t parent_size, 
                          char *out_filename, size_t filename_size) {
    if (!path || path[0] == '\0') return -1;

    char clean[256];
    fs_sanitize_path(path, clean, sizeof(clean));

    if (clean[0] == '\0') return -1;

    if (str_strcmp(clean, "/") == 0) {
        str_strcpy(out_parent, "/");
        str_strcpy(out_filename, ".");
        return 0;
    }

    size_t len = str_strlen(clean);
    int last_slash = -1;

    for (int i = (int)len - 1; i >= 0; i--) {
        if (clean[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash == -1) {
        out_parent[0] = '\0';
        str_strncpy(out_filename, clean, filename_size - 1);
        out_filename[filename_size - 1] = '\0';
        return 0;
    }

    str_strncpy(out_filename, &clean[last_slash + 1], filename_size - 1);
    out_filename[filename_size - 1] = '\0';

    if (last_slash == 0) {
        str_strcpy(out_parent, "/");
    } else {
        size_t copy_len = (size_t)last_slash < (parent_size - 1) ? (size_t)last_slash : (parent_size - 1);
        str_strncpy(out_parent, clean, copy_len);
        out_parent[copy_len] = '\0';
    }

    return 0;
}

static int fs_enter_path(const char *path, char *leaf, size_t leaf_size, char *saved_cwd, int *walked) {
    *walked = 0;
    if (!path || path[0] == '\0') return -1;

    char parent_dir[256];
    if (fs_split_path(path, parent_dir, sizeof(parent_dir), leaf, leaf_size) != 0) {
        return -1;
    }

    if (parent_dir[0] != '\0') {
        char *current = (char*)exfat_get_working_dir();
        if (current) {
            str_strcpy(saved_cwd, current);
            *walked = 1;
        }
        if (exfat_change_directory(parent_dir) != 0) {
            return -1;
        }
    }
    return 0;
}

static void fs_leave_path(char *saved_cwd, int walked) {
    if (walked && saved_cwd) {
        exfat_change_directory(saved_cwd);
    }
}

int fs_set_auth_override(int enabled) {
    (void)enabled;
    return 0;
}

int fs_set_permissions(const char *path, uint32_t owner_uid, uint32_t group_gid,
                       uint32_t owner_mask, uint32_t group_mask, uint32_t other_mask) {
    (void)path; (void)owner_uid; (void)group_gid;
    (void)owner_mask; (void)group_mask; (void)other_mask;
    return 0;
}

int fs_check_access(const char *path, uint32_t requested_mask) {
    (void)path; (void)requested_mask;
    return 0;
}

int fs_format(void) {
    return exfat_wipe_and_format();
}

int fs_create(char* filename) {
    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }

    int rc = exfat_create_node(leaf, false);
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_mkdir(char* foldername) {
    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(foldername, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }

    int rc = exfat_create_node(leaf, true);
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_exists(char* filename) {
    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return 0;
    }

    int rc = (exfat_resolve_entry(leaf, NULL) == 0) ? 1 : 0;
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_delete(char* filename) {
    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }

    int rc = exfat_delete_node(leaf);
    fs_leave_path(saved_cwd, walked);
    return (rc == 0) ? 1 : -1;
}

int fs_delete_last_line(char* filename) {
    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }

    int rc = exfat_truncate_last_line(leaf);
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_read(char* filename, char* output_buffer, uint32_t max_size) {
    if (!output_buffer) return -1;

    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }

    int rc = (int)exfat_read_file(leaf, (uint8_t*)output_buffer, max_size);
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_read_raw(char* filename, uint8_t* output_buffer, uint32_t max_size) {
    if (!output_buffer) return -1;

    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }

    int rc = (int)exfat_read_file(leaf, output_buffer, max_size);
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_write_bytes(char* filename, char* input_buffer, uint32_t size) {
    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }

    int rc = exfat_write_file(leaf, (uint8_t*)input_buffer, (uint64_t)size);
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_write(char* filename, char* input_buffer) {
    if (!input_buffer) return -1;
    return fs_write_bytes(filename, input_buffer, (uint32_t)str_strlen(input_buffer));
}

int fs_list_dir(int hidden) {
    return exfat_print_directory(hidden);
}

int fs_copy(char* source, char* dest) {
    static uint8_t raw_buffer[65536];
    int64_t bytes_read = fs_read_raw(source, raw_buffer, sizeof(raw_buffer));
    if (bytes_read < 0) {
        return -1;
    }

    if (fs_exists(dest)) {
        fs_delete(dest);
    }

    if (fs_create(dest) != 0) {
        return -1;
    }

    if (fs_write_bytes(dest, (char*)raw_buffer, (uint32_t)bytes_read) != 0) {
        return -1;
    }

    return 0;
}

int fs_rename(char* filename, char* newname) {
    if (!filename || filename[0] == '\0' || !newname || newname[0] == '\0') {
        return 0;
    }

    if (!fs_exists(filename)) {
        return 0;
    }

    if (fs_exists(newname)) {
        return 0;
    }

    if (fs_copy(filename, newname) != 0) {
        return 0;
    }

    fs_delete(filename);
    return 1;
}

int fs_chdir(char* folder) {
    if (!folder || folder[0] == '\0') {
        return -1;
    }

    char clean[256];
    fs_sanitize_path(folder, clean, sizeof(clean));
    if (clean[0] == '\0') return -1;

    return exfat_change_directory(clean);
}

char* fs_dirname(void) {
    return (char*)exfat_get_working_dir();
}

int fs_mount(void) {
    return exfat_mount();
}

int fs_list(const char* directory, int show_hidden) {
    if (directory && directory[0] != '\0') {
        char clean[256];
        fs_sanitize_path(directory, clean, sizeof(clean));
        if (clean[0] != '\0') {
            char saved_cwd[256];
            char *current = (char*)exfat_get_working_dir();
            int walked = 0;
            if (current) {
                str_strcpy(saved_cwd, current);
                walked = 1;
            }

            if (exfat_change_directory(clean) != 0) {
                return -1;
            }

            int rc = exfat_print_directory(show_hidden);

            if (walked) {
                exfat_change_directory(saved_cwd);
            }
            return rc;
        }
    }
    return exfat_print_directory(show_hidden);
}

char* fs_current_dir(void) {
    return (char*)exfat_get_working_dir();
}