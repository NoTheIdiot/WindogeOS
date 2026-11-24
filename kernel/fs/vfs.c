#include <dogeio.h>
#include <string.h>
#include <bool.h>

#define MAX_CLUSTER_SIZE 65536
#define MAX_PATH_LEN     256
#define MAX_DIR_ENTRIES  128
#define INT_MAX_VAL      2147483647

#define PERM_READ        (1 << 0)
#define PERM_WRITE       (1 << 1)
#define PERM_EXEC        (1 << 2)
#define PERM_DELETE      (1 << 3)
#define PERM_ADMIN_ONLY  (1 << 7)
#define PERM_DEFAULT     (PERM_READ | PERM_WRITE | PERM_EXEC | PERM_DELETE)

mount_t exfat_vol;

extern int list_dir(mount_t *vol, const char* path, fs_t *out_files, int max_files);
extern uint8_t proc_is_admin(void);

static void split_path(const char *path, char *parent, size_t parent_sz, char *name, size_t name_sz) {
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        get_cwd(parent, (int)parent_sz);
        strncpy(name, path, name_sz - 1);
        name[name_sz - 1] = '\0';
        return;
    }

    if (last_slash == path) {
        strncpy(parent, "/", parent_sz - 1);
        parent[parent_sz - 1] = '\0';
    } else {
        size_t dir_len = (size_t)(last_slash - path);
        if (dir_len >= parent_sz) dir_len = parent_sz - 1;
        memcpy(parent, path, dir_len);
        parent[dir_len] = '\0';
    }

    strncpy(name, last_slash + 1, name_sz - 1);
    name[name_sz - 1] = '\0';
}

static bool fs_find_entry(const char *path, fs_t *out_entry) {
    if (!path || path[0] == '\0') return false;

    if (strcmp(path, "/") == 0) {
        if (out_entry) {
            memset(out_entry, 0, sizeof(fs_t));
            strcpy(out_entry->name, "/");
        }
        return true;
    }

    char parent[MAX_PATH_LEN];
    char target[MAX_PATH_LEN];
    split_path(path, parent, sizeof(parent), target, sizeof(target));

    if (target[0] == '\0') return false;

    static fs_t entries[MAX_DIR_ENTRIES];
    int count = list_dir(&exfat_vol, parent, entries, MAX_DIR_ENTRIES);
    if (count < 0) return false;

    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, target) == 0) {
            if (out_entry) {
                *out_entry = entries[i];
            }
            sprintf("[HDD] file exists: %s\n", path);
            return true;
        }
    }

    sprintf("[HDD] file doesn't exist: %s\n", path);
    return false;
}

static bool fs_check_access(const char *path, uint8_t req_flags) {
    if (!path || path[0] == '\0') return false;
    if (strcmp(path, "/") == 0) return true;

    char parent[MAX_PATH_LEN];
    char target[MAX_PATH_LEN];
    split_path(path, parent, sizeof(parent), target, sizeof(target));
    if (target[0] == '\0') return false;

    uint32_t dir_cluster = exfat_vol.root_cluster;
    if (strcmp(parent, "/") != 0) {
        fs_t parent_entry;
        if (!fs_find_entry(parent, &parent_entry)) return false;
        dir_cluster = parent_entry.first_cluster;
    }

    uint8_t file_perm = get_perm(&exfat_vol, dir_cluster, target, PERM_DEFAULT);
    return (file_perm & req_flags) == req_flags;
}


int fs_mount(void) {
    static uint8_t buffer[MAX_CLUSTER_SIZE];
    int status = mount(&exfat_vol, buffer);
    if (status == 0) {
        uint32_t clear_size = exfat_vol.bytes_per_cluster;
        if (clear_size > sizeof(buffer)) {
            clear_size = sizeof(buffer);
        }
        memset(buffer, 0, clear_size);
    }
    return status;
}

bool fs_exists(char *filename) {
    return fs_find_entry(filename, NULL);
}

uint32_t fs_get_size(char *filename) {
    fs_t entry;
    if (fs_find_entry(filename, &entry)) {
        return entry.size;
    }
    return 0;
}

int fs_create(char* filename) {
    char parent[MAX_PATH_LEN];
    char target[MAX_PATH_LEN];
    split_path(filename, parent, sizeof(parent), target, sizeof(target));
    if (!fs_check_access(parent, PERM_WRITE)) return -1;

    return create_file(&exfat_vol, filename);
}

int fs_write_force(char* filename, char* data, uint64_t size) {
    if (fs_exists(filename)) {
        if (!fs_check_access(filename, PERM_WRITE)) return -1;
    } else {
        char parent[MAX_PATH_LEN];
        char target[MAX_PATH_LEN];
        split_path(filename, parent, sizeof(parent), target, sizeof(target));
        if (!fs_check_access(parent, PERM_WRITE)) return -1;
    }

    return write_file(&exfat_vol, filename, (uint8_t*)data, size);
}

int fs_read(char* filename, char* data, uint64_t size) {
    if (!fs_check_access(filename, PERM_READ)) return -1;

    uint64_t bytes_read = 0;
    if (read_file(&exfat_vol, filename, (uint8_t*)data, size, &bytes_read) != 0) {
        return -1;
    }
    if (bytes_read > INT_MAX_VAL) return -1;
    return (int)bytes_read;
}

int fs_write(char* filename, char* data, uint64_t size) {
    if (!fs_check_access(filename, PERM_WRITE)) return -1;

    while (size > 0 && data[size - 1] == '\0') {
        size--;
    }

    int status = append_file(&exfat_vol, filename, (uint8_t*)data, size);
    if (status != 0) {
        return status;
    }

    char nl = '\n';
    return append_file(&exfat_vol, filename, (uint8_t*)&nl, 1);
}

int fs_mkdir(char* path) {
    char parent[MAX_PATH_LEN];
    char target[MAX_PATH_LEN];
    split_path(path, parent, sizeof(parent), target, sizeof(target));
    if (!fs_check_access(parent, PERM_WRITE)) return -1;

    return create_dir(&exfat_vol, path);
}

int fs_list(char* path, fs_t* out, int max_entries) {
    if (!fs_check_access(path, PERM_READ)) return -1;

    return list_dir(&exfat_vol, path, out, max_entries);
}

int fs_delete(char* filename) {
    if (!fs_check_access(filename, PERM_DELETE)) return -1;

    return delete_file(&exfat_vol, filename);
}

int fs_read_raw(char *filename, uint8_t *buffer, uint64_t size) {
    if (!fs_check_access(filename, PERM_READ)) return -1;

    uint64_t bytes_read = 0;
    if (read_file(&exfat_vol, filename, buffer, size, &bytes_read) != 0) {
        return -1;
    }
    if (bytes_read > INT_MAX_VAL) return -1;
    return (int)bytes_read;
}

int fs_write_raw(char* filename, uint8_t* buffer, uint64_t size) {
    if (fs_exists(filename)) {
        if (!fs_check_access(filename, PERM_WRITE)) return -1;
    } else {
        char parent[MAX_PATH_LEN];
        char target[MAX_PATH_LEN];
        split_path(filename, parent, sizeof(parent), target, sizeof(target));
        if (!fs_check_access(parent, PERM_WRITE)) return -1;
    }

    return write_file(&exfat_vol, filename, buffer, size);
}

int fs_read_raw_at(char *filename, uint8_t *buffer, uint64_t offset, uint64_t size) {
    if (!fs_check_access(filename, PERM_READ)) return -1;

    if (size == 0) {
        return 0;
    }

    if (offset == 0) {
        uint64_t bytes_read = 0;
        if (read_file(&exfat_vol, filename, buffer, size, &bytes_read) != 0) {
            return -1;
        }
        if (bytes_read > INT_MAX_VAL) return -1;
        return (int)bytes_read;
    }

    static uint8_t temp[65536];
    if (offset > sizeof(temp) || size > sizeof(temp) - offset) {
        return -1;
    }
    uint64_t total_needed = offset + size;

    uint64_t bytes_read = 0;
    if (read_file(&exfat_vol, filename, temp, total_needed, &bytes_read) != 0) {
        return -1;
    }

    if (offset >= bytes_read) {
        return 0;
    }

    if (offset + size > bytes_read) {
        size = bytes_read - offset;
    }

    memcpy(buffer, temp + offset, size);
    return (int)size;
}

int fs_chdir(char* path) {
    if (!fs_check_access(path, PERM_READ)) return -1;

    return change_dir(&exfat_vol, path);
}

void fs_get_dir(char* buffer, int size) {
    get_cwd(buffer, size);
}
