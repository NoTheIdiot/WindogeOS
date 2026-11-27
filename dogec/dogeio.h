#ifndef DOGEIO_H
#define DOGEIO_H

#include "stdint.h"
#include "bool.h"
#include "stddef.h"
#include "root.h"

#define KEY_RIGHT 0x80
#define KEY_LEFT  0x81
#define KEY_DOWN  0x82
#define KEY_UP    0x83

#ifndef __DOGE_HAVE_PRINTF
#define __DOGE_HAVE_PRINTF

static inline int printf(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = _root_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0) return -1;
    size_t ulen = (size_t)len;
    if (ulen >= sizeof(buf)) ulen = sizeof(buf) - 1;

    return (int)syscall_3(TEXT, TEXT_PRINTF, (uint64_t)buf, (uint64_t)ulen);
}

#endif /* __DOGE_HAVE_PRINTF */

#ifndef __DOGE_HAVE_IO_HELPERS
#define __DOGE_HAVE_IO_HELPERS

static inline int clear(void) {
    return printf("\033[2J\033[H");
}

static inline void exit(void) {
    root_doge(DOGE_EXIT);
}

#endif /* __DOGE_HAVE_IO_HELPERS */

static inline int64_t doge_err(uint64_t res) {
    if (res == ERR_UNKNOWN_SYSCALL ||
        res == ERR_UNKNOWN_OPCODE  ||
        res == ERR_BAD_PTR         ||
        res == ERR_INVAL) {
        return -(int64_t)(-(int64_t)res);
    }
    if ((int64_t)res < 0) {
        return (int64_t)res;
    }
    return (int64_t)res;
}

static inline bool doge_is_err(uint64_t res) {
    return res == ERR_UNKNOWN_SYSCALL ||
           res == ERR_UNKNOWN_OPCODE  ||
           res == ERR_BAD_PTR         ||
           res == ERR_INVAL           ||
           (int64_t)res < 0;
}

static inline int print_str(const char *s) {
    if (!s) return -1;
    size_t len = 0;
    while (s[len]) len++;
    if (len == 0) return 0;
    uint64_t res = syscall_3(TEXT, TEXT_PRINTF, (uint64_t)s, (uint64_t)len);
    return doge_is_err(res) ? -1 : (int)res;
}

static inline int getkey(void) {
    uint64_t res = root_console(TEXT_GETKEY);
    if (doge_is_err(res)) return -1;
    return (int)(uint8_t)res;
}

static inline int64_t input(char *buf, uint64_t max_len) {
    if (!buf || max_len == 0) return -1;
    uint64_t res = root_console(TEXT_INPUT, buf, max_len);
    if (doge_is_err(res)) return -1;
    return (int64_t)res;
}

static inline int64_t fs_read(const char *filename, void *buf, uint64_t size) {
    if (!filename || !buf || size == 0) return -1;
    uint64_t res = root_fs(FS_READ, filename, buf, size);
    if (doge_is_err(res)) return -1;
    return (int64_t)res;
}

static inline int64_t fs_write(const char *filename, const void *buf, uint64_t size) {
    if (!filename || (!buf && size > 0)) return -1;
    uint64_t res = root_fs(FS_WRITE, filename, buf, size);
    if (doge_is_err(res)) return -1;
    return (int64_t)res;
}

static inline int fs_create(const char *filename) {
    if (!filename) return -1;
    uint64_t res = root_fs(FS_CREATE, filename);
    return doge_is_err(res) ? -1 : 0;
}

static inline int fs_delete(const char *filename) {
    if (!filename) return -1;
    uint64_t res = root_fs(FS_DELETE, filename);
    return doge_is_err(res) ? -1 : 0;
}

static inline int fs_stat(const char *filename, doge_fs_stat_t *st) {
    if (!filename || !st) return -1;
    uint64_t res = root_fs(FS_STAT, filename, st);
    return doge_is_err(res) ? -1 : 0;
}

static inline bool fs_exists(const char *filename) {
    doge_fs_stat_t st = {0};
    return fs_stat(filename, &st) == 0;
}

static inline int64_t fs_size(const char *filename) {
    doge_fs_stat_t st = {0};
    if (fs_stat(filename, &st) != 0) return -1;
    return (int64_t)st.size;
}

static inline int fs_write_all(const char *filename, const void *buf, uint64_t size) {
    if (!filename || (!buf && size > 0)) return -1;
    if (fs_create(filename) != 0) {
        fs_delete(filename);
        if (fs_create(filename) != 0) return -1;
    }
    int64_t w = fs_write(filename, buf, size);
    return (w == (int64_t)size) ? 0 : -1;
}

static inline int64_t fs_read_all(const char *filename, void *buf, uint64_t max_size) {
    if (!filename || !buf || max_size == 0) return -1;
    return fs_read(filename, buf, max_size);
}

static inline int fs_append(const char *filename, const void *buf, uint64_t size) {
    if (!filename || !buf || size == 0) return -1;
    doge_fs_stat_t st = {0};
    uint64_t existing_len = 0;
    if (fs_stat(filename, &st) == 0) {
        existing_len = st.size;
    } else if (fs_create(filename) != 0) {
        return -1;
    }

    if (existing_len + size > 65536ULL) return -1;

    static uint8_t append_buf[65536];
    uint64_t cur = 0;
    if (existing_len > 0) {
        int64_t r = fs_read(filename, append_buf, existing_len);
        if (r < 0) return -1;
        cur = (uint64_t)r;
    }

    const uint8_t *src = (const uint8_t *)buf;
    for (uint64_t i = 0; i < size; i++) {
        append_buf[cur + i] = src[i];
    }

    int64_t w = fs_write(filename, append_buf, cur + size);
    return (w == (int64_t)(cur + size)) ? 0 : -1;
}

static inline void wait(uint64_t time) {
    root_doge(DOGE_SLEEP, time);
}

static inline uint64_t uptime(void) {
    return root_doge(DOGE_UPTIME);
}

static inline int64_t get_time_str(char *buf, uint64_t size) {
    if (!buf || size == 0) return -1;
    uint64_t res = root_doge(DOGE_GET_TIME, buf, size);
    if (doge_is_err(res)) return -1;
    return (int64_t)res;
}

static inline void beep(uint64_t hz, uint64_t time) {
    root_doge(DOGE_BEEP, hz, time);
}

static inline int rand64(uint64_t *out) {
    if (!out) return -1;
    uint64_t res = root_doge(DOGE_RAND, out);
    return doge_is_err(res) ? -1 : 0;
}

static inline uint64_t rand_u64(void) {
    uint64_t v = 0;
    if (rand64(&v) == 0) return v;
    uint64_t x = uptime() ^ ((uint64_t)(uintptr_t)&v * 2685821657736338717ULL);
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 2685821657736338717ULL;
}

static inline uint32_t rand_range(uint32_t min_val, uint32_t max_val) {
    if (max_val <= min_val) return min_val;
    uint32_t span = (max_val - min_val) + 1U;
    return min_val + (uint32_t)(rand_u64() % span);
}

static inline void *sbrk(int64_t inc) {
    uint64_t res = root_mem(MEM_SBRK, inc);
    if (doge_is_err(res)) {
        return NULL;
    }
    return (void *)(uintptr_t)res;
}

static inline void *brk_current(void) {
    return sbrk(0);
}

static inline void *mmap_anon(uint64_t size, uint64_t flags) {
    if (size == 0) return NULL;
    uint64_t res = root_mem(MEM_MAP, 0ULL, size, flags);
    if (doge_is_err(res)) {
        return NULL;
    }
    return (void *)(uintptr_t)res;
}

static inline void *mmap_at(uint64_t virt, uint64_t size, uint64_t flags) {
    if (virt == 0 || size == 0) return NULL;
    uint64_t res = root_mem(MEM_MAP, virt, size, flags);
    if (doge_is_err(res)) {
        return NULL;
    }
    return (void *)(uintptr_t)res;
}

static inline int64_t ipc_send(const void *buf, uint64_t len) {
    if (!buf || len == 0) return -1;
    uint64_t res = root_ipc(IPC_SEND, buf, len);
    if (doge_is_err(res)) return -1;
    return (int64_t)res;
}

static inline int64_t ipc_recv(void *buf, uint64_t max_len) {
    if (!buf || max_len == 0) return -1;
    uint64_t res = root_ipc(IPC_RECV, buf, max_len);
    if (doge_is_err(res)) return -1;
    return (int64_t)res;
}

static inline int64_t ipc_send_str(const char *s) {
    if (!s) return -1;
    size_t len = 0;
    while (s[len]) len++;
    if (len == 0) return 0;
    return ipc_send(s, (uint64_t)len);
}

static inline int64_t exec_prog(const char *filename, uint64_t file_size) {
    if (!filename) return -1;
    uint64_t res = root_proc(PROC_RUN, filename, file_size);
    if (doge_is_err(res)) return -1;
    return (int64_t)res;
}

static inline void proc_exit_now(void) {
    root_proc(PROC_EXIT);
}

static inline int fs_list(const char *path, fs_t *out, int max_entries) {

    if (!path || !out || max_entries <= 0) {
        return -1;
    }

    uint64_t res = root_fs(FS_LISTDIR,
                           path,
                           out,
                           (uint64_t)max_entries);

    if (doge_is_err(res)) {
        return -1;
    }

    return (int)res;
}

static inline int fs_chdir(const char *path) {

    if (!path) {
        return -1;
    }

    uint64_t res = root_fs(FS_CHDIR, path);

    return doge_is_err(res) ? -1 : 0;
}

static inline int fs_get_dir(char *buffer, uint64_t size) {

    if (!buffer || size == 0) {
        return -1;
    }

    uint64_t res = root_fs(FS_GETDIR,
                           buffer,
                           size);

    return doge_is_err(res) ? -1 : 0;
}

static inline int fs_mkdir(const char *path) {

    if (!path) {
        return -1;
    }

    return fs_create(path);
}

static inline int run(char* path, size_t size, int argc, char** argv) {
    return root_proc(PROC_RUN, path, size, argc, argv);
}

static inline void reboot(void) {
    root_doge(DOGE_REBOOT);
}

static inline void shutdown(void) {
    root_doge(DOGE_SHUTDOWN);
}

#endif