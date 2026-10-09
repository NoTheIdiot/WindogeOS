#include <dogeio.h>
#include <system.h>
#include <boot/syscall_internal.h>
#include <boot/limine.h>
#include <string.h>
#include <time.h>

#define LINUX_SYS_READ          0
#define LINUX_SYS_WRITE         1
#define LINUX_SYS_OPEN          2
#define LINUX_SYS_CLOSE         3
#define LINUX_SYS_STAT          4
#define LINUX_SYS_FSTAT         5
#define LINUX_SYS_LSEEK         8
#define LINUX_SYS_MMAP          9
#define LINUX_SYS_MPROTECT      10
#define LINUX_SYS_MUNMAP        11
#define LINUX_SYS_BRK           12
#define LINUX_SYS_PREAD64       17
#define LINUX_SYS_ACCESS        21
#define LINUX_SYS_GETPID        39
#define LINUX_SYS_EXIT           60
#define LINUX_SYS_UNAME          63
#define LINUX_SYS_GETTIMEOFDAY  96
#define LINUX_SYS_GETUID        102
#define LINUX_SYS_GETGID        104
#define LINUX_SYS_GETEUID       107
#define LINUX_SYS_GETEGID       108
#define LINUX_SYS_TIME          201
#define LINUX_SYS_CLOCK_GETTIME 228
#define LINUX_SYS_EXIT_GROUP    231
#define LINUX_SYS_GETCWD        79
#define LINUX_SYS_OPENAT        257
#define LINUX_SYS_NEWFSTATAT    262

#define LINUX_O_WRONLY   0x0001
#define LINUX_O_RDWR     0x0002
#define LINUX_O_CREAT    0x0040
#define LINUX_O_TRUNC    0x0200
#define LINUX_O_APPEND   0x0400
#define LINUX_AT_FDCWD   ((uint64_t)-100)
#define LINUX_AT_SYMLINK_NOFOLLOW 0x0100
#define LINUX_AT_EMPTY_PATH       0x1000

#define LINUX_SEEK_SET   0
#define LINUX_SEEK_CUR   1
#define LINUX_SEEK_END   2

#define LINUX_PROT_WRITE 0x2
#define LINUX_PROT_EXEC  0x4
#define LINUX_PROT_NONE  0x0
#define LINUX_MAP_FIXED  0x10
#define LINUX_MAP_ANON   0x20
#define LINUX_MAP_PRIVATE 0x02
#define LINUX_MAP_SHARED  0x01

#define LINUX_EBADF      9
#define LINUX_EIO        5
#define LINUX_EFAULT     14
#define LINUX_EINVAL     22
#define LINUX_EMFILE     24
#define LINUX_ENOENT     2
#define LINUX_ENOMEM     12
#define LINUX_EISDIR     21
#define LINUX_EACCES     13
#define LINUX_ESPIPE     29
#define LINUX_ENOSYS     38
#define LINUX_EOVERFLOW  75
#define LINUX_EFBIG      27

#define USER_IO_LIMIT       65536ULL
#define USER_PATH_LIMIT     256
#define USER_PAGE_SIZE      4096ULL
#define USER_HEAP_BASE      0x0000600000000000ULL
#define USER_MMAP_BASE      0x0000700000000000ULL
#define USER_MMAP_LIMIT     0x00007F0000000000ULL
#define USER_ADDRESS_LIMIT  0x0000800000000000ULL
#define LINUX_FD_LIMIT      32
#define LINUX_FILE_IO_LIMIT 65536ULL
#define LINUX_CLOCK_REALTIME 0
#define LINUX_CLOCK_MONOTONIC 1

typedef struct {
    bool open;
    bool readable;
    bool writable;
    bool append;
    bool is_dir;
    bool is_terminal;
    uint64_t offset;
    char path[USER_PATH_LIMIT];
} linux_file_descriptor_t;

typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    int32_t st_pad0;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    int64_t st_atime;
    uint64_t st_atime_nsec;
    int64_t st_mtime;
    uint64_t st_mtime_nsec;
    int64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t st_unused[3];
} linux_stat_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_usec;
} linux_timeval_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_nsec;
} linux_timespec_t;

typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} linux_utsname_t;

static linux_file_descriptor_t file_descriptors[LINUX_FD_LIMIT];
static uint64_t linux_program_break;
static uint64_t linux_mmap_cursor;
static uint64_t linux_heap_start;
static uint8_t linux_file_image[LINUX_FILE_IO_LIMIT];
static uint8_t linux_write_data[USER_IO_LIMIT];
extern volatile struct limine_hhdm_request hhdm_request;

static uint64_t linux_error(uint64_t error) {
    return (uint64_t)-(int64_t)error;
}

static uint64_t align_up_page(uint64_t value) {
    return (value + USER_PAGE_SIZE - 1) & ~(USER_PAGE_SIZE - 1);
}

static void linux_reset_process_state(void) {
    memset(file_descriptors, 0, sizeof(file_descriptors));
    file_descriptors[0].open = true;
    file_descriptors[0].readable = true;
    file_descriptors[0].is_terminal = true;
    file_descriptors[1].open = true;
    file_descriptors[1].writable = true;
    file_descriptors[1].is_terminal = true;
    file_descriptors[2].open = true;
    file_descriptors[2].writable = true;
    file_descriptors[2].is_terminal = true;
    linux_heap_start = USER_HEAP_BASE;
    linux_program_break = linux_heap_start;
    linux_mmap_cursor = USER_MMAP_BASE;
}

void linux_syscall_reset(void) {
    linux_reset_process_state();
}

void linux_syscall_set_heap_base(uint64_t address) {
    if (address > linux_heap_start && address < USER_MMAP_BASE - USER_PAGE_SIZE) {
        linux_heap_start = align_up_page(address);
        linux_program_break = linux_heap_start;
    }
}

static linux_file_descriptor_t *get_file_descriptor(uint64_t fd) {
    if (fd >= LINUX_FD_LIMIT || !file_descriptors[fd].open) {
        return NULL;
    }
    return &file_descriptors[fd];
}

static uint64_t get_file_info(const char *path, uint64_t *size, bool *is_dir) {
    return fs_get_info((char *)path, size, is_dir) == 0 ? 0 : linux_error(LINUX_ENOENT);
}

static void fill_linux_stat(linux_stat_t *stat_out, uint64_t size, bool is_dir) {
    memset(stat_out, 0, sizeof(*stat_out));
    stat_out->st_ino = size;
    stat_out->st_nlink = 1;
    stat_out->st_mode = is_dir ? 0040755U : 0100644U;
    stat_out->st_uid = 0;
    stat_out->st_gid = 0;
    stat_out->st_size = (int64_t)size;
    stat_out->st_blksize = 4096;
    stat_out->st_blocks = (int64_t)((size + 511) / 512);
}

static uint64_t linux_stat_fd(linux_file_descriptor_t *fd, uint64_t user_stat) {
    if (fd == NULL) {
        return linux_error(LINUX_EBADF);
    }
    if (user_stat == 0 ||
        !user_range_accessible(user_stat, sizeof(linux_stat_t), true)) {
        return linux_error(LINUX_EFAULT);
    }
    if (fd->is_terminal) {
        linux_stat_t *stat_out = (linux_stat_t *)user_stat;
        memset(stat_out, 0, sizeof(*stat_out));
        stat_out->st_mode = 0020666U;
        stat_out->st_nlink = 1;
        return 0;
    }
    uint64_t file_size;
    bool is_dir;
    if (get_file_info(fd->path, &file_size, &is_dir) != 0) {
        return linux_error(LINUX_ENOENT);
    }
    fill_linux_stat((linux_stat_t *)user_stat, file_size, is_dir);
    return 0;
}

static uint64_t linux_stat_path(uint64_t user_path, uint64_t user_stat) {
    char path[USER_PATH_LIMIT];
    uint64_t file_size;
    bool is_dir;
    if (!copy_user_string(user_path, path, sizeof(path), NULL) ||
        user_stat == 0 || !user_range_accessible(user_stat, sizeof(linux_stat_t), true)) {
        return linux_error(LINUX_EFAULT);
    }
    if (get_file_info(path, &file_size, &is_dir) != 0) {
        return linux_error(LINUX_ENOENT);
    }
    fill_linux_stat((linux_stat_t *)user_stat, file_size, is_dir);
    return 0;
}

static uint64_t linux_open_file(char *path, uint64_t flags) {
    bool writable = (flags & (LINUX_O_WRONLY | LINUX_O_RDWR)) != 0;
    bool readable = (flags & LINUX_O_WRONLY) == 0;
    uint64_t file_size = 0;
    bool is_dir = false;
    if (get_file_info(path, &file_size, &is_dir) != 0) {
        if ((flags & LINUX_O_CREAT) == 0) {
            return linux_error(LINUX_ENOENT);
        }
        if (fs_create(path) != 0) {
            return linux_error(LINUX_EACCES);
        }
        file_size = 0;
        is_dir = false;
    } else if ((flags & LINUX_O_TRUNC) != 0) {
        if (!writable || is_dir ||
            fs_write_bytes(path, "", 0) != 0) {
            return linux_error(is_dir ? LINUX_EISDIR : LINUX_EACCES);
        }
        file_size = 0;
    }

    if (is_dir && writable) {
        return linux_error(LINUX_EISDIR);
    }
    if (is_dir && (flags & LINUX_O_TRUNC) != 0) {
        return linux_error(LINUX_EISDIR);
    }

    for (uint64_t fd = 0; fd < LINUX_FD_LIMIT; fd++) {
        if (file_descriptors[fd].open) {
            continue;
        }
        linux_file_descriptor_t *entry = &file_descriptors[fd];
        entry->open = true;
        entry->readable = readable;
        entry->writable = writable;
        entry->append = (flags & LINUX_O_APPEND) != 0;
        entry->is_dir = is_dir;
        entry->offset = entry->append ? file_size : 0;
        size_t path_length = 0;
        while (path[path_length] != '\0' && path_length + 1 < sizeof(entry->path)) {
            entry->path[path_length] = path[path_length];
            path_length++;
        }
        entry->path[path_length] = '\0';
        return fd;
    }
    return linux_error(LINUX_EMFILE);
}

static uint64_t linux_read_fd(linux_file_descriptor_t *fd, uint64_t buffer,
                              uint64_t count, bool positional, uint64_t offset) {
    if (!fd->readable) {
        return linux_error(LINUX_EBADF);
    }
    if (count > USER_IO_LIMIT) {
        count = USER_IO_LIMIT;
    }
    if (count == 0) {
        return 0;
    }
    if (!user_range_accessible(buffer, count, true)) {
        return linux_error(LINUX_EFAULT);
    }
    if (fd->is_dir) {
        return linux_error(LINUX_EISDIR);
    }
    if (fd->is_terminal && fd == &file_descriptors[0]) {
        if (count > 256) {
            count = 256;
        }
        char input[257];
        dogeio_text_input("", input, (size_t)count + 1);
        size_t read_count;
        size_t input_capacity = (size_t)count + 1;
        size_t index = 0;
        while (index < input_capacity && input[index] != '\0') {
            index++;
        }
        read_count = index;
        if (read_count > count) {
            read_count = (size_t)count;
        }
        memcpy((void *)buffer, input, read_count);
        return (uint64_t)read_count;
    }
    int bytes_read = fs_read_raw_at(fd->path, (uint8_t *)buffer,
                                    positional ? offset : fd->offset, count);
    if (bytes_read < 0) {
        return linux_error(LINUX_EIO);
    }
    if (!positional) {
        fd->offset += (uint64_t)bytes_read;
    }
    return (uint64_t)bytes_read;
}

static uint64_t linux_write_fd(linux_file_descriptor_t *fd, uint64_t buffer,
                               uint64_t count) {
    if (!fd->writable) {
        return linux_error(LINUX_EBADF);
    }
    if (count > USER_IO_LIMIT) {
        count = USER_IO_LIMIT;
    }
    if (count == 0) {
        return 0;
    }
    if (!user_range_accessible(buffer, count, false)) {
        return linux_error(LINUX_EFAULT);
    }
    if (fd->is_terminal &&
        (fd == &file_descriptors[1] || fd == &file_descriptors[2])) {
        for (uint64_t i = 0; i < count; i++) {
            dogeio_text_printchar(((const char *)buffer)[i]);
        }
        return count;
    }
    if (fd->is_dir) {
        return linux_error(LINUX_EISDIR);
    }

    uint64_t file_size;
    bool is_dir;
    if (get_file_info(fd->path, &file_size, &is_dir) != 0) {
        return linux_error(LINUX_ENOENT);
    }
    if (is_dir) {
        return linux_error(LINUX_EISDIR);
    }
    if (fd->append) {
        fd->offset = file_size;
    }
    if (fd->offset > LINUX_FILE_IO_LIMIT ||
        count > LINUX_FILE_IO_LIMIT - fd->offset ||
        file_size > LINUX_FILE_IO_LIMIT) {
        return linux_error(LINUX_EFBIG);
    }
    memcpy(linux_write_data, (const void *)buffer, (size_t)count);
    if (file_size != 0) {
        int bytes_read = fs_read_raw_at(fd->path, linux_file_image, 0, file_size);
        if (bytes_read < 0 || (uint64_t)bytes_read != file_size) {
            return linux_error(LINUX_EIO);
        }
    }
    uint64_t new_size = file_size;
    if (fd->offset + count > new_size) {
        new_size = fd->offset + count;
    }
    if (fd->offset > file_size) {
        memset(linux_file_image + file_size, 0,
               (size_t)(fd->offset - file_size));
    }
    memcpy(linux_file_image + fd->offset, linux_write_data, (size_t)count);
    int result = fs_write_bytes(fd->path, (char *)linux_file_image,
                                (uint32_t)new_size);
    if (result != 0) {
        return linux_error(LINUX_EACCES);
    }
    fd->offset += count;
    return count;
}

static uint64_t linux_map_memory(uint64_t requested_address, uint64_t length,
                                 uint64_t protection, uint64_t flags,
                                 linux_file_descriptor_t *fd, uint64_t offset) {
    if (length == 0 || length > 16ULL * 1024 * 1024 ||
        length > USER_ADDRESS_LIMIT - USER_PAGE_SIZE ||
        (flags & (LINUX_MAP_PRIVATE | LINUX_MAP_SHARED)) == 0 ||
        (flags & (LINUX_MAP_PRIVATE | LINUX_MAP_SHARED)) ==
            (LINUX_MAP_PRIVATE | LINUX_MAP_SHARED)) {
        return linux_error(LINUX_EINVAL);
    }
    if ((flags & LINUX_MAP_ANON) == 0 &&
        (fd == NULL || !fd->open || !fd->readable || fd->is_terminal ||
         (offset & (USER_PAGE_SIZE - 1)) != 0)) {
        return linux_error(LINUX_EBADF);
    }
    if ((flags & LINUX_MAP_SHARED) != 0 && (flags & LINUX_MAP_ANON) == 0) {
        return linux_error(LINUX_ENOSYS);
    }
    if ((protection & ~7ULL) != 0) {
        return linux_error(LINUX_EINVAL);
    }

    uint64_t map_length = align_up_page(length);
    if (map_length < length) {
        return linux_error(LINUX_ENOMEM);
    }
    if ((flags & LINUX_MAP_ANON) == 0 &&
        offset > UINT64_MAX - map_length) {
        return linux_error(LINUX_EOVERFLOW);
    }
    bool fixed = (flags & LINUX_MAP_FIXED) != 0;
    uint64_t address = requested_address;
    if (fixed) {
        if ((address & (USER_PAGE_SIZE - 1)) != 0 ||
            address < USER_PAGE_SIZE ||
            address >= USER_MMAP_LIMIT ||
            map_length > USER_MMAP_LIMIT - address) {
            return linux_error(LINUX_EINVAL);
        }
    } else {
        address = align_up_page(address);
        if (address < USER_MMAP_BASE || address >= USER_MMAP_LIMIT ||
            map_length > USER_MMAP_LIMIT - address) {
            address = align_up_page(linux_mmap_cursor);
        }
        while (address < USER_MMAP_LIMIT &&
               map_length <= USER_MMAP_LIMIT - address) {
            bool available = true;
            for (uint64_t page = address; page < address + map_length;
                 page += USER_PAGE_SIZE) {
                if (user_page_mapped(page)) {
                    available = false;
                    break;
                }
            }
            if (available) {
                break;
            }
            address += map_length;
        }
        if (address >= USER_MMAP_LIMIT ||
            map_length > USER_MMAP_LIMIT - address) {
            return linux_error(LINUX_ENOMEM);
        }
    }

    for (uint64_t page = address; page < address + map_length;
         page += USER_PAGE_SIZE) {
        if (!fixed && user_page_mapped(page)) {
            return linux_error(LINUX_EINVAL);
        }
        if (fixed) {
            (void)unmap_user_page(page);
        }
    }

    for (uint64_t page = address; page < address + map_length;
         page += USER_PAGE_SIZE) {
        uint64_t physical = pmm_alloc_zeroed_page();
        map_user_page(page, physical);
        if ((flags & LINUX_MAP_ANON) == 0 && offset <= UINT64_MAX - (page - address)) {
            uint64_t file_offset = offset + (page - address);
            int read_count = fs_read_raw_at(fd->path,
                (uint8_t *)(physical + hhdm_request.response->offset),
                file_offset, USER_PAGE_SIZE);
            if (read_count < 0) {
                for (uint64_t rollback = address; rollback <= page;
                     rollback += USER_PAGE_SIZE) {
                    (void)unmap_user_page(rollback);
                }
                return linux_error(LINUX_EINVAL);
            }
        }
        if (!protect_user_page(page, protection != LINUX_PROT_NONE,
                               (protection & LINUX_PROT_WRITE) != 0,
                               (protection & LINUX_PROT_EXEC) != 0)) {
            for (uint64_t rollback = address; rollback <= page;
                 rollback += USER_PAGE_SIZE) {
                (void)unmap_user_page(rollback);
            }
            return linux_error(LINUX_EINVAL);
        }
    }
    if (!fixed) {
        linux_mmap_cursor = address + map_length;
    }
    return address;
}

uint64_t linux_syscall_handler(syscall_registers_t *regs) {
    switch (regs->rax) {
        case LINUX_SYS_READ: {
            linux_file_descriptor_t *fd = get_file_descriptor(regs->rdi);
            return fd == NULL ? linux_error(LINUX_EBADF) :
                   linux_read_fd(fd, regs->rsi, regs->rdx, false, 0);
        }
        case LINUX_SYS_WRITE: {
            linux_file_descriptor_t *fd = get_file_descriptor(regs->rdi);
            return fd == NULL ? linux_error(LINUX_EBADF) :
                   linux_write_fd(fd, regs->rsi, regs->rdx);
        }
        case LINUX_SYS_OPEN: {
            char path[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, path, sizeof(path), NULL)) {
                return linux_error(LINUX_EFAULT);
            }
            return linux_open_file(path, regs->rsi);
        }
        case LINUX_SYS_OPENAT: {
            char path[USER_PATH_LIMIT];
            if (regs->rdi != LINUX_AT_FDCWD) {
                return linux_error(LINUX_EBADF);
            }
            if (!copy_user_string(regs->rsi, path, sizeof(path), NULL)) {
                return linux_error(LINUX_EFAULT);
            }
            return linux_open_file(path, regs->rdx);
        }
        case LINUX_SYS_CLOSE: {
            if (regs->rdi >= LINUX_FD_LIMIT || !file_descriptors[regs->rdi].open) {
                return linux_error(LINUX_EBADF);
            }
            memset(&file_descriptors[regs->rdi], 0,
                   sizeof(file_descriptors[regs->rdi]));
            return 0;
        }
        case LINUX_SYS_STAT:
            return linux_stat_path(regs->rdi, regs->rsi);
        case LINUX_SYS_FSTAT: {
            linux_file_descriptor_t *fd = get_file_descriptor(regs->rdi);
            return linux_stat_fd(fd, regs->rsi);
        }
        case LINUX_SYS_LSEEK: {
            linux_file_descriptor_t *fd = get_file_descriptor(regs->rdi);
            if (fd == NULL) {
                return linux_error(LINUX_EBADF);
            }
            if (fd->is_terminal) {
                return linux_error(LINUX_ESPIPE);
            }
            int64_t base;
            if (regs->rdx == LINUX_SEEK_SET) {
                base = 0;
            } else if (regs->rdx == LINUX_SEEK_CUR) {
                if (fd->offset > INT64_MAX) {
                    return linux_error(LINUX_EOVERFLOW);
                }
                base = (int64_t)fd->offset;
            } else if (regs->rdx == LINUX_SEEK_END) {
                uint64_t file_size;
                bool is_dir;
                if (get_file_info(fd->path, &file_size, &is_dir) != 0 ||
                    file_size > INT64_MAX) {
                    return linux_error(LINUX_EINVAL);
                }
                base = (int64_t)file_size;
            } else {
                return linux_error(LINUX_EINVAL);
            }
            int64_t displacement = (int64_t)regs->rsi;
            if ((displacement > 0 && base > INT64_MAX - displacement) ||
                (displacement < 0 && base < INT64_MIN - displacement) ||
                base + displacement < 0) {
                return linux_error(LINUX_EINVAL);
            }
            fd->offset = (uint64_t)(base + displacement);
            return fd->offset;
        }
        case LINUX_SYS_PREAD64: {
            linux_file_descriptor_t *fd = get_file_descriptor(regs->rdi);
            return fd == NULL ? linux_error(LINUX_EBADF) :
                   linux_read_fd(fd, regs->rsi, regs->rdx, true, regs->r10);
        }
        case LINUX_SYS_ACCESS: {
            char path[USER_PATH_LIMIT];
            uint64_t file_size;
            bool is_dir;
            if (!copy_user_string(regs->rdi, path, sizeof(path), NULL)) {
                return linux_error(LINUX_EFAULT);
            }
            if (get_file_info(path, &file_size, &is_dir) != 0) {
                return linux_error(LINUX_ENOENT);
            }
            (void)file_size;
            (void)is_dir;
            return regs->rsi <= 7 ? 0 : linux_error(LINUX_EINVAL);
        }
        case LINUX_SYS_MMAP: {
            if (hhdm_request.response == NULL) {
                return linux_error(LINUX_ENOMEM);
            }
            linux_file_descriptor_t *fd = NULL;
            if ((regs->r10 & LINUX_MAP_ANON) == 0) {
                fd = get_file_descriptor(regs->r8);
            }
            return linux_map_memory(regs->rdi, regs->rsi, regs->rdx,
                                    regs->r10, fd, regs->r9);
        }
        case LINUX_SYS_MPROTECT: {
            if (regs->rsi == 0 || regs->rdi >= USER_ADDRESS_LIMIT ||
                (regs->rdx & ~7ULL) != 0 ||
                (regs->rdi & (USER_PAGE_SIZE - 1)) != 0 ||
                regs->rsi > USER_ADDRESS_LIMIT - regs->rdi) {
                return linux_error(LINUX_EINVAL);
            }
            uint64_t end = align_up_page(regs->rdi + regs->rsi);
            for (uint64_t page = regs->rdi; page < end; page += USER_PAGE_SIZE) {
                if (!user_page_mapped(page)) {
                    return linux_error(LINUX_ENOMEM);
                }
                if (!user_page_accessible(page, false) &&
                    !protect_user_page(page, true,
                                       (regs->rdx & LINUX_PROT_WRITE) != 0,
                                       (regs->rdx & LINUX_PROT_EXEC) != 0)) {
                    return linux_error(LINUX_ENOMEM);
                }
                if (user_page_accessible(page, false) &&
                    !protect_user_page(page, regs->rdx != LINUX_PROT_NONE,
                                       (regs->rdx & LINUX_PROT_WRITE) != 0,
                                       (regs->rdx & LINUX_PROT_EXEC) != 0)) {
                    return linux_error(LINUX_ENOMEM);
                }
            }
            return 0;
        }
        case LINUX_SYS_MUNMAP: {
            if (regs->rsi == 0 ||
                (regs->rdi & (USER_PAGE_SIZE - 1)) != 0 ||
                regs->rsi > USER_ADDRESS_LIMIT - regs->rdi) {
                return linux_error(LINUX_EINVAL);
            }
            uint64_t end = align_up_page(regs->rdi + regs->rsi);
            for (uint64_t page = regs->rdi; page < end; page += USER_PAGE_SIZE) {
                (void)unmap_user_page(page);
            }
            return 0;
        }
        case LINUX_SYS_BRK: {
            if (regs->rdi == 0) {
                return linux_program_break;
            }
            if (regs->rdi < linux_heap_start ||
                regs->rdi >= USER_MMAP_BASE - USER_PAGE_SIZE) {
                return linux_program_break;
            }
            uint64_t old_end = align_up_page(linux_program_break);
            uint64_t new_end = align_up_page(regs->rdi);
            if (new_end > old_end) {
                for (uint64_t page = old_end; page < new_end;
                     page += USER_PAGE_SIZE) {
                    if (user_page_mapped(page)) {
                        return linux_program_break;
                    }
                }
                for (uint64_t page = old_end; page < new_end;
                     page += USER_PAGE_SIZE) {
                    map_user_page(page, pmm_alloc_zeroed_page());
                }
            } else if (new_end < old_end) {
                for (uint64_t page = new_end; page < old_end;
                     page += USER_PAGE_SIZE) {
                    (void)unmap_user_page(page);
                }
            }
            linux_program_break = regs->rdi;
            return linux_program_break;
        }
        case LINUX_SYS_NEWFSTATAT: {
            if ((regs->r10 & ~(uint64_t)(LINUX_AT_SYMLINK_NOFOLLOW |
                                         LINUX_AT_EMPTY_PATH)) != 0) {
                return linux_error(LINUX_EINVAL);
            }
            char path[USER_PATH_LIMIT];
            if ((regs->r10 & LINUX_AT_EMPTY_PATH) != 0 &&
                regs->rsi != 0 &&
                copy_user_string(regs->rsi, path, sizeof(path), NULL) &&
                path[0] == '\0') {
                return linux_stat_fd(get_file_descriptor(regs->rdi), regs->rdx);
            }
            if (regs->rdi != LINUX_AT_FDCWD) {
                return linux_error(LINUX_EBADF);
            }
            return linux_stat_path(regs->rsi, regs->rdx);
        }
        case LINUX_SYS_GETCWD: {
            const char *cwd = fs_dirname();
            if (cwd == NULL || regs->rsi == 0) {
                return linux_error(LINUX_EINVAL);
            }
            size_t length = str_strlen(cwd) + 1;
            if ((uint64_t)length > regs->rsi ||
                !user_range_accessible(regs->rdi, (uint64_t)length, true)) {
                return linux_error(LINUX_EFAULT);
            }
            memcpy((char *)regs->rdi, cwd, length);
            return regs->rdi;
        }
        case LINUX_SYS_GETPID:
            return 1;
        case LINUX_SYS_GETUID:
        case LINUX_SYS_GETEUID:
        case LINUX_SYS_GETGID:
        case LINUX_SYS_GETEGID:
            return 0;
        case LINUX_SYS_UNAME: {
            static const linux_utsname_t uname_info = {
                "WindogeOS", "windoge", "0.1", "WindogeOS", "x86_64", ""
            };
            if (regs->rdi == 0 ||
                !user_range_accessible(regs->rdi, sizeof(uname_info), true)) {
                return linux_error(LINUX_EFAULT);
            }
            memcpy((void *)regs->rdi, &uname_info, sizeof(uname_info));
            return 0;
        }
        case LINUX_SYS_GETTIMEOFDAY: {
            uint64_t epoch = time_get_epoch_seconds();
            if (regs->rdi != 0) {
                if (!user_range_accessible(regs->rdi, sizeof(linux_timeval_t), true)) {
                    return linux_error(LINUX_EFAULT);
                }
                linux_timeval_t *tv = (linux_timeval_t *)regs->rdi;
                tv->tv_sec = (int64_t)epoch;
                tv->tv_usec = 0;
            }
            if (regs->rsi != 0) {
                if (!user_range_accessible(regs->rsi, 8, true)) {
                    return linux_error(LINUX_EFAULT);
                }
                memset((void *)regs->rsi, 0, 8);
            }
            return 0;
        }
        case LINUX_SYS_TIME: {
            uint64_t epoch = time_get_epoch_seconds();
            if (regs->rdi != 0) {
                if (!user_range_accessible(regs->rdi, sizeof(epoch), true)) {
                    return linux_error(LINUX_EFAULT);
                }
                *(uint64_t *)regs->rdi = epoch;
            }
            return epoch;
        }
        case LINUX_SYS_CLOCK_GETTIME: {
            if (regs->rdi != LINUX_CLOCK_REALTIME &&
                regs->rdi != LINUX_CLOCK_MONOTONIC) {
                return linux_error(LINUX_EINVAL);
            }
            if (regs->rsi == 0 ||
                !user_range_accessible(regs->rsi, sizeof(linux_timespec_t), true)) {
                return linux_error(LINUX_EFAULT);
            }
            linux_timespec_t *ts = (linux_timespec_t *)regs->rsi;
            ts->tv_sec = (int64_t)time_get_epoch_seconds();
            ts->tv_nsec = 0;
            return 0;
        }
        case LINUX_SYS_EXIT:
        case LINUX_SYS_EXIT_GROUP:
            if (kernel_program_launcher_rsp == 0) {
                return linux_error(LINUX_EINVAL);
            }
            syscall_set_linux_abi(false);
            cleanup_user_pages();
            syscall_exit_to_launcher(regs->rdi);
        default:
            return linux_error(LINUX_ENOSYS);
    }
}
