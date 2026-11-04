#include <dogeio.h>
#include <basicutil.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <bool.h>
#include <system.h>
#include "disk.h"

extern int exfat_resolve_entry(const char *target_name, void *out);
extern int exfat_get_info(const char *name, uint64_t *size, bool *is_dir);
extern int exfat_wipe_and_format(void);
extern int exfat_create_node(const char *name, bool is_dir);
extern int exfat_delete_node(const char *name);
extern int exfat_truncate_last_line(const char *name);
extern int64_t exfat_read_file(const char *name, uint8_t *buffer, uint64_t max_size);
extern int64_t exfat_read_file_at(const char *name, uint8_t *buffer, uint64_t offset, uint64_t max_size);
extern int exfat_write_file(const char *name, const uint8_t *buffer, uint64_t size);
extern int exfat_append_file(const char *name, const uint8_t *data, uint64_t size);
extern int exfat_print_directory(int hidden);
extern int exfat_list_entries(char (*names)[256], size_t capacity);
extern int exfat_list_files(char (*names)[256], size_t capacity);
extern int exfat_change_directory(const char *path);
extern const char* exfat_get_working_dir(void);
extern int exfat_mount(void);

static uint8_t g_current_drive = ATA_PRIMARY_MASTER;
static uint8_t g_drive_mounted[ATA_DRIVE_COUNT];
static char g_drive_working_dirs[ATA_DRIVE_COUNT][256] = {
    "/", "/"
};
static char g_current_drive_path[260];

static int fs_mount_or_format_drive(uint8_t drive) {
    int was_mounted = g_drive_mounted[drive] != 0;
    if (!ata_drive_present(drive) || ata_select_drive(drive) != 0) {
        return -1;
    }

    int mount_result = exfat_mount();
    if (mount_result == 0) {
        log("[storage] WARNING: formatting an unformatted drive erases its data");
        log(drive == ATA_PRIMARY_MASTER
                ? "[storage] C: is unformatted; formatting exFAT"
                : "[storage] D: is unformatted; formatting exFAT");
        if (exfat_wipe_and_format() != 0) {
            log("[storage] automatic drive format failed");
            return -1;
        }
        mount_result = exfat_mount();
    }
    if (mount_result != 1) {
        log("[storage] drive mount failed; refusing to format after an I/O or geometry error");
        return -1;
    }

    g_drive_mounted[drive] = 1;
    if (!was_mounted) {
        str_strcpy(g_drive_working_dirs[drive], "/");
    }
    return 0;
}

static int fs_select_drive_internal(uint8_t drive) {
    if (drive >= ATA_DRIVE_COUNT || !ata_drive_present(drive)) {
        return -1;
    }
    if (drive == g_current_drive && g_drive_mounted[drive]) {
        return 0;
    }

    const char *current_dir = exfat_get_working_dir();
    if (g_drive_mounted[g_current_drive] && current_dir != NULL &&
        str_strlen(current_dir) < sizeof(g_drive_working_dirs[0])) {
        str_strcpy(g_drive_working_dirs[g_current_drive], current_dir);
    }

    uint8_t previous_drive = g_current_drive;
    if (fs_mount_or_format_drive(drive) != 0) {
        if (previous_drive < ATA_DRIVE_COUNT &&
            ata_drive_present(previous_drive)) {
            (void)ata_select_drive(previous_drive);
            g_current_drive = previous_drive;
            if (g_drive_mounted[previous_drive]) {
                (void)exfat_mount();
                (void)exfat_change_directory(
                    g_drive_working_dirs[previous_drive]);
            }
        }
        return -1;
    }
    g_current_drive = drive;
    if (exfat_change_directory(g_drive_working_dirs[drive]) != 0) {
        str_strcpy(g_drive_working_dirs[drive], "/");
        if (exfat_change_directory("/") != 0) {
            g_drive_mounted[drive] = 0;
            return -1;
        }
    }
    return 0;
}

static int fs_prepare_path(const char *path, char *out, size_t out_size) {
    if (path == NULL || out == NULL || out_size == 0) {
        return -1;
    }

    if ((path[0] >= 'a' && path[0] <= 'z') &&
        path[1] == ':') {
        char drive_letter = path[0];
        if (drive_letter >= 'a' && drive_letter <= 'z') {
            drive_letter = (char)(drive_letter - 'a' + 'a');
        }
        if (drive_letter != 'c' && drive_letter != 'd') {
            return -1;
        }
        if (fs_select_drive_internal((uint8_t)(drive_letter - 'c')) != 0) {
            return -1;
        }
        path += 2;
        if (*path == '\0') {
            path = "/";
        }
    }

    if (!g_drive_mounted[g_current_drive] ||
        str_strncpy(out, path, out_size) != 0) {
        return -1;
    }
    return 0;
}

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
        size_t parent_length = (size_t)last_slash;
        if (parent_length >= parent_size) {
            return -1;
        }
        for (size_t i = 0; i < parent_length; i++) {
            out_parent[i] = clean[i];
        }
        out_parent[parent_length] = '\0';
    }

    return 0;
}

static int fs_enter_path(const char *path, char *leaf, size_t leaf_size, char *saved_cwd, int *walked) {
    *walked = 0;
    if (!path || path[0] == '\0') return -1;

    char resolved_path[256];
    if (fs_prepare_path(path, resolved_path, sizeof(resolved_path)) != 0) {
        return -1;
    }

    char parent_dir[256];
    if (fs_split_path(resolved_path, parent_dir, sizeof(parent_dir), leaf, leaf_size) != 0) {
        return -1;
    }

    if (parent_dir[0] != '\0') {
        const char *current = exfat_get_working_dir();
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
    if (!ata_drive_present(g_current_drive) ||
        exfat_wipe_and_format() != 0 ||
        exfat_mount() != 1) {
        g_drive_mounted[g_current_drive] = 0;
        return -1;
    }
    g_drive_mounted[g_current_drive] = 1;
    str_strcpy(g_drive_working_dirs[g_current_drive], "/");
    return 0;
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

int fs_get_info(char *filename, uint64_t *size, bool *is_dir) {
    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (filename == NULL || size == NULL || is_dir == NULL) {
        return -1;
    }

    char resolved_path[256];
    if (fs_prepare_path(filename, resolved_path, sizeof(resolved_path)) != 0) {
        return -1;
    }
    if (str_strcmp(resolved_path, "/") == 0 ||
        str_strcmp(resolved_path, ".") == 0) {
        *size = 0;
        *is_dir = true;
        return 0;
    }

    if (fs_enter_path(resolved_path, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }
    int rc = exfat_get_info(leaf, size, is_dir);
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_isdir(char *filename) {
    uint64_t size = 0;
    bool is_dir = false;

    if (fs_get_info(filename, &size, &is_dir) != 0) {
        return 0;
    }
    return is_dir ? 1 : 0;
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

int fs_read(char* filename, char* output_buffer, uint64_t max_size) {
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

int fs_read_raw(char* filename, uint8_t* output_buffer, uint64_t max_size) {
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

int fs_read_raw_at(char* filename, uint8_t* output_buffer, uint64_t offset, uint64_t max_size) {
    if (!output_buffer) return -1;

    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if (fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }

    int rc = (int)exfat_read_file_at(leaf, output_buffer, offset, max_size);
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
    if (!g_drive_mounted[g_current_drive]) {
        return -1;
    }
    return exfat_print_directory(hidden);
}

static int fs_list_entries_common(const char *directory,
                                  char (*names)[256], size_t capacity,
                                  bool include_directories) {
    if (directory == NULL || directory[0] == '\0' ||
        (names == NULL && capacity != 0)) {
        return -1;
    }

    char resolved_directory[256];
    if (fs_prepare_path(directory, resolved_directory,
                        sizeof(resolved_directory)) != 0) {
        return -1;
    }

    char saved_cwd[256];
    const char *current = exfat_get_working_dir();
    if (current == NULL || str_strlen(current) >= sizeof(saved_cwd)) {
        return -1;
    }
    str_strcpy(saved_cwd, current);

    if (exfat_change_directory(resolved_directory) != 0) {
        return -1;
    }

    int count = include_directories ? exfat_list_entries(names, capacity)
                                    : exfat_list_files(names, capacity);
    if (exfat_change_directory(saved_cwd) != 0) {
        return -1;
    }
    if (count < 0 || (size_t)count > capacity) {
        return -1;
    }
    return count;
}

int fs_list_files(const char *directory, char (*names)[256], size_t capacity) {
    return fs_list_entries_common(directory, names, capacity, false);
}

int fs_list_entries(const char *directory, char (*names)[256], size_t capacity) {
    return fs_list_entries_common(directory, names, capacity, true);
}

int fs_append_data(char *filename, const uint8_t *data, uint32_t size) {
    char leaf[256];
    char saved_cwd[256];
    int walked = 0;

    if ((data == NULL && size != 0) ||
        fs_enter_path(filename, leaf, sizeof(leaf), saved_cwd, &walked) != 0) {
        return -1;
    }
    int rc = exfat_append_file(leaf, data, size);
    fs_leave_path(saved_cwd, walked);
    return rc;
}

int fs_copy(char* source, char* dest) {
    uint8_t raw_buffer[4096];
    uint64_t offset = 0;

    if (fs_exists(dest)) {
        if (fs_delete(dest) != 1) {
            return -1;
        }
    }

    if (fs_create(dest) != 0) {
        return -1;
    }

    while (true) {
        int bytes_read = fs_read_raw_at(source, raw_buffer, offset,
                                        sizeof(raw_buffer));
        if (bytes_read < 0) {
            fs_delete(dest);
            return -1;
        }
        if (bytes_read == 0) {
            break;
        }
        if (fs_append_data(dest, raw_buffer, (uint32_t)bytes_read) != 0) {
            fs_delete(dest);
            return -1;
        }
        offset += (uint64_t)bytes_read;
    }

    return 0;
}

int fs_move(char* source, char* dest) {
    if (source == NULL || source[0] == '\0' ||
        dest == NULL || dest[0] == '\0' ||
        str_strcmp(source, dest) == 0 ||
        !fs_exists(source) || fs_exists(dest)) {
        return -1;
    }

    if (fs_copy(source, dest) != 0) {
        return -1;
    }

    if (fs_delete(source) != 1) {
        fs_delete(dest);
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

    uint8_t previous_drive = g_current_drive;
    char clean[256];
    if (fs_prepare_path(folder, clean, sizeof(clean)) != 0) {
        return -1;
    }
    if (clean[0] == '\0') {
        str_strcpy(clean, "/");
    }
    fs_sanitize_path(clean, clean, sizeof(clean));
    if (clean[0] == '\0') return -1;

    if (exfat_change_directory(clean) != 0) {
        if (g_current_drive != previous_drive) {
            (void)fs_select_drive_internal(previous_drive);
        }
        return -1;
    }
    str_strcpy(g_drive_working_dirs[g_current_drive], exfat_get_working_dir());
    return 0;
}

int fs_select_drive(uint8_t drive) {
    return fs_select_drive_internal(drive);
}

int fs_drive_present(uint8_t drive) {
    return ata_drive_present(drive);
}

uint8_t fs_current_drive(void) {
    return g_current_drive;
}

char* fs_dirname(void) {
    if (!g_drive_mounted[g_current_drive]) {
        return NULL;
    }
    const char *working_dir = exfat_get_working_dir();
    if (working_dir == NULL) {
        return NULL;
    }
    g_current_drive_path[0] = (char)('c' + g_current_drive);
    g_current_drive_path[1] = ':';
    str_strcpy(g_current_drive_path + 2, working_dir);
    return g_current_drive_path;
}

int fs_mount(void) {
    ata_probe_devices();
    g_current_drive = ATA_PRIMARY_MASTER;
    g_drive_mounted[ATA_PRIMARY_MASTER] = 0;
    g_drive_mounted[ATA_PRIMARY_SLAVE] = 0;
    str_strcpy(g_drive_working_dirs[ATA_PRIMARY_MASTER], "/");
    str_strcpy(g_drive_working_dirs[ATA_PRIMARY_SLAVE], "/");

    for (uint8_t drive = 0; drive < ATA_DRIVE_COUNT; drive++) {
        if (ata_drive_present(drive) &&
            fs_mount_or_format_drive(drive) != 0) {
            log(drive == ATA_PRIMARY_MASTER
                    ? "[storage] C: is unavailable"
                    : "[storage] D: is unavailable");
        }
    }

    if (!g_drive_mounted[ATA_PRIMARY_MASTER] ||
        ata_select_drive(ATA_PRIMARY_MASTER) != 0 ||
        exfat_mount() != 1 ||
        exfat_change_directory(g_drive_working_dirs[ATA_PRIMARY_MASTER]) != 0) {
        return 0;
    }
    return 1;
}

int fs_list(const char* directory, int show_hidden) {
    if (!directory || directory[0] == '\0') {
        if (!g_drive_mounted[g_current_drive]) {
            return -1;
        }
        return exfat_print_directory(show_hidden);
    }

    char clean[256];
    if (fs_prepare_path(directory, clean, sizeof(clean)) != 0) {
        return -1;
    }
    fs_sanitize_path(clean, clean, sizeof(clean));

    if (str_strcmp(clean, "--hidden") == 0 || clean[0] == '\0') {
        return exfat_print_directory(show_hidden);
    }

    char saved_cwd[256];
    const char *current = exfat_get_working_dir();
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

char* fs_current_dir(void) {
    return fs_dirname();
}