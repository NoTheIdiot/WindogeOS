#include <dogeio.h>
#include <string.h>

#define USERS_DB_PATH "/system/users.dat"

static user_t user_table[MAX_USERS];
static uint8_t current_uid = 0;

static uint64_t hash_password(const char *pw) {
    uint64_t h = 14695981039346656037ULL;
    while (*pw) {
        h ^= (uint8_t)(*pw++);
        h *= 1099511628211ULL;
    }
    return h;
}

static void save_users_db(void) {
    fs_write_raw(USERS_DB_PATH, (uint8_t *)user_table, sizeof(user_table));
}

void users_init(void) {
    memset(user_table, 0, sizeof(user_table));
    current_uid = 0;

    if (!fs_exists("/system")) {
        fs_mkdir("/system");
    }

    if (fs_read_raw(USERS_DB_PATH, (uint8_t *)user_table, sizeof(user_table)) != sizeof(user_table)) {
        memset(user_table, 0, sizeof(user_table));
        user_table[0].active    = 1;
        user_table[0].uid       = 0;
        user_table[0].is_admin  = 1;
        strcpy(user_table[0].username, "admin");
        user_table[0].pass_hash = hash_password("admin");
        save_users_db();
    }
}

int user_login(const char *username, const char *password) {
    if (!username || !password) return -1;

    uint64_t h = hash_password(password);
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].active && strcmp(user_table[i].username, username) == 0) {
            if (user_table[i].pass_hash == h) {
                current_uid = user_table[i].uid;
                return (int)current_uid;
            }
            return -1;
        }
    }
    return -1;
}

void user_logout(void) {
    current_uid = USER_INVALID;
}

uint8_t user_current_uid(void) {
    return current_uid;
}

uint8_t user_is_admin(void) {
    if (current_uid == USER_INVALID) return 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].active && user_table[i].uid == current_uid) {
            return user_table[i].is_admin;
        }
    }
    return 0;
}

const char *user_current_name(void) {
    if (current_uid == USER_INVALID) return "guest";
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].active && user_table[i].uid == current_uid) {
            return user_table[i].username;
        }
    }
    return "unknown";
}

int user_add(const char *username, const char *password, uint8_t is_admin) {
    if (!user_is_admin()) return -1;
    if (!username || !password || username[0] == '\0') return -1;

    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].active && strcmp(user_table[i].username, username) == 0) {
            return -2;
        }
    }

    for (int i = 0; i < MAX_USERS; i++) {
        if (!user_table[i].active) {
            user_table[i].active    = 1;
            user_table[i].uid       = (uint8_t)i;
            user_table[i].is_admin  = is_admin ? 1 : 0;
            strncpy(user_table[i].username, username, MAX_USERNAME - 1);
            user_table[i].username[MAX_USERNAME - 1] = '\0';
            user_table[i].pass_hash = hash_password(password);
            save_users_db();
            return i;
        }
    }
    return -3;
}

int user_remove(const char *username) {
    if (!user_is_admin() || !username) return -1;

    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].active && strcmp(user_table[i].username, username) == 0) {
            if (user_table[i].uid == 0) return -2;
            memset(&user_table[i], 0, sizeof(user_t));
            save_users_db();
            return 0;
        }
    }
    return -3;
}

int user_passwd(const char *username, const char *new_password) {
    if (!username || !new_password) return -1;

    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].active && strcmp(user_table[i].username, username) == 0) {
            if (!user_is_admin() && user_table[i].uid != current_uid) return -1;
            user_table[i].pass_hash = hash_password(new_password);
            save_users_db();
            return 0;
        }
    }
    return -2;
}
