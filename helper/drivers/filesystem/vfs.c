#include <boot/kernel.h>
#include <dogeio.h>
#include <basicutil.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <bool.h>
#include <system.h>

extern int exfat_resolve_entry(const char *target_name, void *out);

static fs_acl_t g_acl_table[64];
static uint32_t g_acl_count = 0;
static uint32_t g_current_uid = 0;
static uint32_t g_current_gid = 0;
static int g_auth_override = 0;

static int fs_acl_find(const char *path) {
    if (!path || path[0] == '\0') {
        return -1;
    }

    for (uint32_t i = 0; i < g_acl_count; i++) {
        if (str_strcmp(g_acl_table[i].path, path) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void fs_acl_set_default_entry(const char *path, uint32_t owner_uid, uint32_t group_gid,
                                     uint32_t owner_mask, uint32_t group_mask, uint32_t other_mask) {
    if (!path || path[0] == '\0' || g_acl_count >= 64) {
        return;
    }

    int idx = fs_acl_find(path);
    if (idx >= 0) {
        fs_acl_t *entry = &g_acl_table[idx];
        entry->owner_uid = owner_uid;
        entry->group_gid = group_gid;
        entry->owner_mask = owner_mask;
        entry->group_mask = group_mask;
        entry->other_mask = other_mask;
        return;
    }

    str_strcpy(g_acl_table[g_acl_count].path, path);
    g_acl_table[g_acl_count].owner_uid = owner_uid;
    g_acl_table[g_acl_count].group_gid = group_gid;
    g_acl_table[g_acl_count].owner_mask = owner_mask;
    g_acl_table[g_acl_count].group_mask = group_mask;
    g_acl_table[g_acl_count].other_mask = other_mask;
    g_acl_count++;
}

int fs_set_current_user(uint32_t uid, uint32_t gid) {
    g_current_uid = uid;
    g_current_gid = gid;
    return 0;
}

int fs_set_auth_override(int enabled) {
    g_auth_override = enabled ? 1 : 0;
    return 0;
}

int fs_set_permissions(const char *path, uint32_t owner_uid, uint32_t group_gid,
                       uint32_t owner_mask, uint32_t group_mask, uint32_t other_mask) {
    if (!path || path[0] == '\0') {
        return -1;
    }

    fs_acl_set_default_entry(path, owner_uid, group_gid, owner_mask, group_mask, other_mask);
    return 0;
}

int fs_check_access(const char *path, uint32_t requested_mask) {
    if (!path || path[0] == '\0' || requested_mask == 0) {
        return 0;
    }

    if (g_current_uid == 0 || g_auth_override) {
        return 0;
    }

    if (str_strcmp(path, "/system") == 0 || str_startswith(path, "/system/")) {
        return -1;
    }

    if ((str_strcmp(path, ".windoge") == 0 || str_strcmp(path, "/.windoge") == 0) &&
        (requested_mask & FS_PERM_DELETE) != 0) {
        return -1;
    }

    if (str_startswith(path, "/users/")) {
        const char *rest = path + 7;
        while (*rest == '/') rest++;
        if (*rest != '\0') {
            char target_user[64];
            int i = 0;
            while (*rest != '\0' && *rest != '/' && i < 63) {
                target_user[i++] = *rest++;
            }
            target_user[i] = '\0';

            if (str_strcmp(target_user, current_user) != 0 &&
                str_strcmp(target_user, "root") != 0 &&
                str_strcmp(target_user, "admin") != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int fs_format(void) {
    return exfat_wipe_and_format();
}

int fs_create(char* filename) {
    if (filename == NULL || filename[0] == '\0') {
        return -1;
    }

    if (fs_check_access(filename, FS_PERM_CREATE | FS_PERM_WRITE) != 0) {
        dogeio_text_println("vfs: permission denied for create");
        return -1;
    }

    return exfat_create_node(filename, false);
}

int fs_mkdir(char* foldername) {
    if (foldername == NULL || foldername[0] == '\0') {
        return -1;
    }

    if (fs_check_access(foldername, FS_PERM_CREATE | FS_PERM_WRITE) != 0) {
        dogeio_text_println("vfs: permission denied for mkdir");
        return -1;
    }

    return exfat_create_node(foldername, true); 
}

int fs_exists(char* filename) {
    if (!filename || filename[0] == '\0') return 0;
    return (exfat_resolve_entry(filename, NULL) == 0) ? 1 : 0;
}

int fs_delete(char* filename) {
    if (!filename || filename[0] == '\0') {
        return -1;
    }

    if ((str_strcmp(filename, ".windoge") == 0 || str_strcmp(filename, "/.windoge") == 0) ||
        (str_strcmp(filename, "/system") == 0 || str_startswith(filename, "/system/"))) {
        dogeio_text_println("vfs: protected path cannot be deleted");
        return -1;
    }

    if (fs_check_access(filename, FS_PERM_DELETE) != 0) {
        dogeio_text_println("vfs: permission denied for delete");
        return -1;
    }

    if (!fs_exists(filename)) {
        dogeio_text_println("fs (delete): file does not exist");
        return 0;
    }

    if (exfat_delete_node(filename) == 0) {
        dogeio_text_println("fs (delete): file deleted");
        return 1;
    }

    dogeio_text_println("fs (delete): error deleting file");
    return -1;
}

int fs_delete_last_line(char* filename) {
    return exfat_truncate_last_line(filename);
}

int fs_read(char* filename, char* output_buffer, uint32_t max_size) {
    if (!output_buffer) return -1;
    if (!filename || filename[0] == '\0') return -1;

    if (fs_check_access(filename, FS_PERM_READ) != 0) {
        dogeio_text_println("vfs: permission denied for read");
        return -1;
    }

    if (!fs_exists(filename)) {
        dogeio_text_println("vfs: file not found or read error.");
        return -1;
    }

    return (int)exfat_read_file(filename, (uint8_t*)output_buffer, max_size);
}

int fs_write_bytes(char* filename, char* input_buffer, uint32_t size) {
    if (!filename || filename[0] == '\0') {
        dogeio_text_println("vfs: invalid filename for write.");
        return -1;
    }

    if (fs_check_access(filename, FS_PERM_WRITE) != 0) {
        dogeio_text_println("vfs: permission denied for write");
        return -1;
    }

    if (!fs_exists(filename)) {
        dogeio_text_println("vfs: file not found for write.");
        return -2;
    }

    return exfat_write_file(filename, (uint8_t*)input_buffer, (uint64_t)size);
}

int fs_write(char* filename, char* input_buffer) {
    if (!input_buffer) return -1;
    return fs_write_bytes(filename, input_buffer, (uint32_t)str_strlen(input_buffer));
}

int fs_list_dir(int hidden) {
    char *cwd = fs_dirname();
    if (cwd != NULL && str_strcmp(cwd, "/users") == 0 &&
        str_strcmp(current_user, "root") != 0 && str_strcmp(current_user, "admin") != 0) {
        dogeio_text_println("vfs: listing /users is restricted to your account.");
        return -1;
    }

    if (str_startswith(cwd, "/system") && str_strcmp(current_user, "root") != 0 && str_strcmp(current_user, "admin") != 0) {
        return -1;
    }

    return exfat_print_directory(hidden);
}

int fs_rename(char* filename, char* newname) {
    if (!filename || filename[0] == '\0' || !newname || newname[0] == '\0') {
        dogeio_text_println("Error: invalid rename arguments.");
        return 0;
    }

    if (fs_check_access(filename, FS_PERM_DELETE) != 0 || fs_check_access(newname, FS_PERM_CREATE | FS_PERM_WRITE) != 0) {
        dogeio_text_println("Error: permission denied for rename.");
        return 0;
    }

    if (!fs_exists(filename)) {
        dogeio_text_println("Error: source file does not exist.");
        return 0;
    }

    if (fs_exists(newname)) {
        dogeio_text_println("Error: target file already exists.");
        return 0;
    }

    fs_copy(filename, newname);
    fs_delete(filename);
    return 1;
}

void fs_copy(char* source, char* dest) {
    static uint8_t raw_buffer[8192];
    int64_t bytes_read = exfat_read_file(source, raw_buffer, sizeof(raw_buffer) - 1);
    if (bytes_read < 0) {
        dogeio_text_println("vfs: copy source file not found or unreadable.");
        return;
    }

    if (fs_check_access(dest, FS_PERM_CREATE | FS_PERM_WRITE) != 0) {
        dogeio_text_println("vfs: permission denied for copy destination");
        return;
    }

    if (fs_exists(dest)) {
        fs_delete(dest);
    }
    fs_create(dest);

    exfat_write_file(dest, raw_buffer, (uint64_t)bytes_read);
}

int fs_chdir(char* folder) {
    if (!folder || folder[0] == '\0') {
        return -1;
    }

    if (fs_check_access(folder, FS_PERM_EXEC | FS_PERM_LIST) != 0) {
        dogeio_text_println("vfs: permission denied for chdir");
        return -1;
    }

    return exfat_change_directory(folder);
}

char* fs_dirname(void) {
    return (char*)exfat_get_working_dir();
}

int fs_mount(void) {
    int rc = exfat_mount();
    if (rc == 0) {
        g_acl_count = 0;
        fs_acl_set_default_entry("/", 0, 0, 0x1Fu, 0x1Fu, 0x05u);
        fs_acl_set_default_entry("/system", 0, 0, 0x1Fu, 0x1Fu, 0x00u);
        fs_acl_set_default_entry("/users", 0, 0, 0x1Fu, 0x1Fu, 0x05u);
    }
    return rc;
}