#ifndef STDIO_H
#define STDIO_H

#include "stdint.h"
#include "bool.h"
#include "stddef.h"
#include "root.h"
#include <stdarg.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define FOPEN_MAX 16
#define FILENAME_MAX 256
#define _STDIO_SCRATCH_MAX 65536
#define _STDIO_LINE_BUF_MAX 256

typedef enum {
    _STDIO_STREAM_FILE = 0,
    _STDIO_STREAM_STDIN,
    _STDIO_STREAM_STDOUT,
    _STDIO_STREAM_STDERR
} _stdio_stream_kind_t;

typedef struct {
    char path[FILENAME_MAX];
    uint64_t offset;
    bool in_use;
    bool can_read;
    bool can_write;
    bool append;
    bool eof;
    bool error;
    _stdio_stream_kind_t kind;
} FILE;

static FILE _stdio_files[FOPEN_MAX];

static FILE _stdio_stdin_stream = {
    .path = "<stdin>",
    .offset = 0,
    .in_use = true,
    .can_read = true,
    .can_write = false,
    .append = false,
    .eof = false,
    .error = false,
    .kind = _STDIO_STREAM_STDIN
};

static FILE _stdio_stdout_stream = {
    .path = "<stdout>",
    .offset = 0,
    .in_use = true,
    .can_read = false,
    .can_write = true,
    .append = false,
    .eof = false,
    .error = false,
    .kind = _STDIO_STREAM_STDOUT
};

static FILE _stdio_stderr_stream = {
    .path = "<stderr>",
    .offset = 0,
    .in_use = true,
    .can_read = false,
    .can_write = true,
    .append = false,
    .eof = false,
    .error = false,
    .kind = _STDIO_STREAM_STDERR
};

#define stdin  (&_stdio_stdin_stream)
#define stdout (&_stdio_stdout_stream)
#define stderr (&_stdio_stderr_stream)

static inline bool _stdio_is_err(uint64_t res) {
    return res == ERR_UNKNOWN_SYSCALL ||
           res == ERR_UNKNOWN_OPCODE  ||
           res == ERR_BAD_PTR         ||
           res == ERR_INVAL           ||
           (int64_t)res < 0;
}

static inline uint64_t _stdio_file_size(const char *path) {
    doge_fs_stat_t st = {0};
    if (!_stdio_is_err(root_fs(FS_STAT, path, &st))) {
        return st.size;
    }
    return 0;
}

static inline int vprintf(const char *fmt, va_list args) {
    if (!fmt) return EOF;
    char buf[512];
    int len = _root_vsnprintf(buf, sizeof(buf), fmt, args);
    if (len < 0) return EOF;
    size_t ulen = (size_t)len;
    if (ulen >= sizeof(buf)) ulen = sizeof(buf) - 1;
    uint64_t res = syscall_3(TEXT, TEXT_PRINTF, (uint64_t)buf, (uint64_t)ulen);
    return _stdio_is_err(res) ? EOF : (int)res;
}

#ifndef __DOGE_HAVE_PRINTF
#define __DOGE_HAVE_PRINTF

static inline int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = vprintf(fmt, args);
    va_end(args);
    return r;
}

#endif /* __DOGE_HAVE_PRINTF */

static inline int snprintf(char *str, size_t size, const char *fmt, ...) {
    if (!str || size == 0 || !fmt) return -1;
    va_list args;
    va_start(args, fmt);
    int r = _root_vsnprintf(str, (uint64_t)size, fmt, args);
    va_end(args);
    return r;
}

static inline int vsnprintf(char *str, size_t size, const char *fmt, va_list args) {
    if (!str || size == 0 || !fmt) return -1;
    return _root_vsnprintf(str, (uint64_t)size, fmt, args);
}

static inline int puts(const char *s) {
    if (!s) return EOF;
    return printf("%s\n", s);
}

static inline int putchar(int c) {
    char ch = (char)c;
    uint64_t res = syscall_3(TEXT, TEXT_PRINTF, (uint64_t)&ch, 1);
    return (res == 1) ? (unsigned char)ch : EOF;
}

static inline int getchar(void) {
    uint64_t res = root_console(TEXT_GETKEY);
    if (_stdio_is_err(res)) return EOF;
    return (int)(uint8_t)res;
}

static inline FILE *fopen(const char *filename, const char *mode) {
    if (!filename || !mode) return NULL;

    bool read = false, write = false, append = false, plus = false;
    for (const char *m = mode; *m; m++) {
        if (*m == 'r') read = true;
        else if (*m == 'w') write = true;
        else if (*m == 'a') append = true;
        else if (*m == '+') plus = true;
    }

    if (!read && !write && !append) return NULL;

    FILE *fp = NULL;
    for (int i = 0; i < FOPEN_MAX; i++) {
        if (!_stdio_files[i].in_use) {
            fp = &_stdio_files[i];
            break;
        }
    }
    if (!fp) return NULL;

    size_t i = 0;
    for (; i < FILENAME_MAX - 1 && filename[i]; i++) {
        fp->path[i] = filename[i];
    }
    if (filename[i] != '\0') return NULL;
    fp->path[i] = '\0';

    uint64_t initial_offset = 0;

    if (write) {
        root_fs(FS_DELETE, fp->path);
        if (_stdio_is_err(root_fs(FS_CREATE, fp->path))) {
            return NULL;
        }
    } else if (append) {
        root_fs(FS_CREATE, fp->path);
        initial_offset = _stdio_file_size(fp->path);
    } else if (read) {
        doge_fs_stat_t st = {0};
        if (_stdio_is_err(root_fs(FS_STAT, fp->path, &st))) {
            return NULL;
        }
    }

    fp->in_use = true;
    fp->can_read = read || plus;
    fp->can_write = write || append || plus;
    fp->append = append;
    fp->offset = initial_offset;
    fp->eof = false;
    fp->error = false;
    fp->kind = _STDIO_STREAM_FILE;
    return fp;
}

static inline int fclose(FILE *stream) {
    if (!stream || stream->kind != _STDIO_STREAM_FILE || !stream->in_use) return EOF;
    stream->in_use = false;
    return 0;
}

static inline size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || !stream || !stream->in_use || !stream->can_read || size == 0 || nmemb == 0) {
        return 0;
    }

    uint64_t total = (uint64_t)size * (uint64_t)nmemb;

    if (stream->kind == _STDIO_STREAM_STDIN) {
        uint8_t *dst = (uint8_t *)ptr;
        uint64_t got = 0;
        while (got < total) {
            int ch = getchar();
            if (ch == EOF) {
                stream->eof = true;
                break;
            }
            dst[got++] = (uint8_t)ch;
            if ((char)ch == '\n' || (char)ch == '\r') {
                break;
            }
        }
        stream->offset += got;
        return (size_t)(got / size);
    }

    if (stream->kind != _STDIO_STREAM_FILE) {
        stream->error = true;
        return 0;
    }

    if (stream->offset == 0) {
        uint64_t raw_res = root_fs(FS_READ, stream->path, ptr, total);
        if (_stdio_is_err(raw_res)) {
            stream->error = true;
            return 0;
        }
        uint64_t bytes = raw_res;
        stream->offset += bytes;
        if (bytes < total) stream->eof = true;
        return (size_t)(bytes / size);
    }

    uint64_t fetch_len = stream->offset + total;
    static uint8_t scratch[_STDIO_SCRATCH_MAX];
    if (fetch_len > sizeof(scratch)) fetch_len = sizeof(scratch);

    uint64_t raw_res = root_fs(FS_READ, stream->path, scratch, fetch_len);
    if (_stdio_is_err(raw_res)) {
        stream->error = true;
        return 0;
    }

    uint64_t available = (raw_res > stream->offset) ? (raw_res - stream->offset) : 0;
    if (available > total) available = total;

    uint8_t *dst = (uint8_t *)ptr;
    for (uint64_t i = 0; i < available; i++) {
        dst[i] = scratch[stream->offset + i];
    }

    stream->offset += available;
    if (available < total) stream->eof = true;
    return (size_t)(available / size);
}

static inline size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || !stream || !stream->in_use || !stream->can_write || size == 0 || nmemb == 0) {
        return 0;
    }

    uint64_t total = (uint64_t)size * (uint64_t)nmemb;

    if (stream->kind == _STDIO_STREAM_STDOUT || stream->kind == _STDIO_STREAM_STDERR) {
        uint64_t res = syscall_3(TEXT, TEXT_PRINTF, (uint64_t)ptr, total);
        if (_stdio_is_err(res)) {
            stream->error = true;
            return 0;
        }
        stream->offset += res;
        return (size_t)(res / size);
    }

    if (stream->kind != _STDIO_STREAM_FILE) {
        stream->error = true;
        return 0;
    }

    if (stream->append || stream->offset > 0) {
        static uint8_t existing_buf[_STDIO_SCRATCH_MAX];
        static uint8_t combined_buf[_STDIO_SCRATCH_MAX];

        uint64_t raw_existing = root_fs(FS_READ, stream->path, existing_buf, sizeof(existing_buf));
        uint64_t cur_size = (!_stdio_is_err(raw_existing)) ? raw_existing : 0;

        uint64_t write_pos = stream->append ? cur_size : stream->offset;
        if (write_pos + total > sizeof(combined_buf)) {
            stream->error = true;
            return 0;
        }

        uint64_t final_size = (write_pos + total > cur_size) ? (write_pos + total) : cur_size;

        for (uint64_t i = 0; i < final_size; i++) {
            combined_buf[i] = (i < cur_size) ? existing_buf[i] : 0;
        }

        const uint8_t *src = (const uint8_t *)ptr;
        for (uint64_t i = 0; i < total; i++) {
            combined_buf[write_pos + i] = src[i];
        }

        uint64_t raw_write = root_fs(FS_WRITE, stream->path, combined_buf, final_size);
        if (_stdio_is_err(raw_write)) {
            stream->error = true;
            return 0;
        }

        stream->offset = write_pos + total;
        stream->eof = false;
        return nmemb;
    }

    uint64_t raw_write = root_fs(FS_WRITE, stream->path, ptr, total);
    if (_stdio_is_err(raw_write)) {
        stream->error = true;
        return 0;
    }

    uint64_t written = raw_write;
    stream->offset += written;
    return (size_t)(written / size);
}

static inline int fseek(FILE *stream, long offset, int whence) {
    if (!stream || stream->kind != _STDIO_STREAM_FILE || !stream->in_use) return -1;

    uint64_t base = 0;
    if (whence == SEEK_SET) {
        base = 0;
    } else if (whence == SEEK_CUR) {
        base = stream->offset;
    } else if (whence == SEEK_END) {
        base = _stdio_file_size(stream->path);
    } else {
        return -1;
    }

    int64_t target = (int64_t)base + (int64_t)offset;
    if (target < 0) return -1;

    stream->offset = (uint64_t)target;
    stream->eof = false;
    return 0;
}

static inline long ftell(FILE *stream) {
    if (!stream || !stream->in_use) return -1L;
    return (long)stream->offset;
}

static inline int fputc(int c, FILE *stream) {
    unsigned char ch = (unsigned char)c;
    return (fwrite(&ch, 1, 1, stream) == 1) ? (int)ch : EOF;
}

static inline int fputs(const char *s, FILE *stream) {
    if (!s || !stream) return EOF;
    size_t len = 0;
    while (s[len]) len++;
    if (len == 0) return 0;
    return (fwrite(s, 1, len, stream) == len) ? 0 : EOF;
}

static inline int fgetc(FILE *stream) {
    if (!stream || !stream->in_use || !stream->can_read) return EOF;
    unsigned char ch = 0;
    if (fread(&ch, 1, 1, stream) != 1) return EOF;
    return (int)ch;
}

static inline char *fgets(char *s, int size, FILE *stream) {
    if (!s || size <= 0 || !stream || !stream->in_use || !stream->can_read) {
        return NULL;
    }

    if (stream->kind == _STDIO_STREAM_STDIN) {
        int cap = (size - 1 < _STDIO_LINE_BUF_MAX - 1) ? (size - 1) : (_STDIO_LINE_BUF_MAX - 1);
        char line[_STDIO_LINE_BUF_MAX];
        uint64_t raw_res = root_console(TEXT_INPUT, line, (uint64_t)(cap + 1));
        if (_stdio_is_err(raw_res)) {
            stream->error = true;
            return NULL;
        }
        int i = 0;
        while (i < cap && line[i] != '\0') {
            s[i] = line[i];
            i++;
        }
        if (i < size - 1) {
            s[i++] = '\n';
        }
        s[i] = '\0';
        stream->offset += (uint64_t)i;
        return s;
    }

    int pos = 0;
    while (pos < size - 1) {
        unsigned char ch = 0;
        size_t n = fread(&ch, 1, 1, stream);
        if (n == 0) {
            if (pos == 0) return NULL;
            break;
        }
        s[pos++] = (char)ch;
        if (ch == '\n') break;
    }
    s[pos] = '\0';
    return s;
}

static inline int remove(const char *filename) {
    if (!filename) return -1;
    return _stdio_is_err(root_fs(FS_DELETE, filename)) ? -1 : 0;
}

static inline int rename(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath) return -1;
    doge_fs_stat_t st = {0};
    if (_stdio_is_err(root_fs(FS_STAT, oldpath, &st))) return -1;
    if (st.size > _STDIO_SCRATCH_MAX) return -1;

    static uint8_t buf[_STDIO_SCRATCH_MAX];
    uint64_t got = 0;
    if (st.size > 0) {
        uint64_t raw_r = root_fs(FS_READ, oldpath, buf, st.size);
        if (_stdio_is_err(raw_r)) return -1;
        got = raw_r;
    }

    root_fs(FS_DELETE, newpath);
    if (_stdio_is_err(root_fs(FS_CREATE, newpath))) return -1;
    if (got > 0) {
        uint64_t raw_w = root_fs(FS_WRITE, newpath, buf, got);
        if (_stdio_is_err(raw_w) || raw_w != got) return -1;
    }
    return _stdio_is_err(root_fs(FS_DELETE, oldpath)) ? -1 : 0;
}

static inline int feof(FILE *stream) {
    return (stream && stream->in_use && stream->eof) ? 1 : 0;
}

static inline int ferror(FILE *stream) {
    return (stream && stream->in_use && stream->error) ? 1 : 0;
}

static inline void clearerr(FILE *stream) {
    if (stream && stream->in_use) {
        stream->eof = false;
        stream->error = false;
    }
}

static inline void rewind(FILE *stream) {
    if (stream && stream->kind == _STDIO_STREAM_FILE && stream->in_use) {
        stream->offset = 0;
        stream->eof = false;
        stream->error = false;
    }
}

static inline int fflush(FILE *stream) {
    if (!stream || !stream->in_use) return EOF;
    return 0;
}

static inline int vfprintf(FILE *stream, const char *fmt, va_list args) {
    if (!stream || !stream->in_use || !stream->can_write || !fmt) return EOF;
    char buf[512];
    int len = _root_vsnprintf(buf, sizeof(buf), fmt, args);
    if (len < 0) return EOF;
    size_t ulen = (size_t)len;
    if (ulen >= sizeof(buf)) ulen = sizeof(buf) - 1;
    return (fwrite(buf, 1, ulen, stream) == ulen) ? (int)ulen : EOF;
}

static inline int fprintf(FILE *stream, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = vfprintf(stream, fmt, args);
    va_end(args);
    return r;
}

#endif