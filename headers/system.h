#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <bool.h>
#include <stddef.h>

#define MAX_FLAT_BINARY_SIZE (16 * 1024 * 1024)

int system_dogeshell_ex(char* command);
int system_bash_ex(char* command);
void system_dogeshell();
void system_bash();
void system_settings(void);
void system_load_settings(void);
void system_start_default_shell(void);

static inline int system(char* command) {
    return system_dogeshell_ex(command);
}

typedef struct {
    const char *label;
    const char *value;
    uint32_t color;
} settings_color_t;

typedef struct {
    const char *label;
    const char *value;
    const uint8_t *font_ptr;
} settings_font_t;

void system_fetch();
extern char *windoge_version;
extern char current_user[64];
extern uint32_t old;
extern const char* dogeshell_version;
extern char computer_name[64];

uint64_t get_ram_end_address(void);
uint64_t get_ram(void);
int system_create_user(char* name, char* password, int permission_id);
int system_verify_user(const char* name, char* password);
int system_can_access_path(const char* username, const char* target_path);
void system_run_exec(char *filename, int program_size);
void system_run_bin_args(char *filename, int program_size, int argc, char **argv);
void system_run_bin_args_impl(char *filename, int program_size, int argc, char **argv);
extern void to_userland_ring3_args(uint64_t user_rip, uint64_t user_rsp,
                                   uint64_t argc, uint64_t argv)
    __attribute__((noreturn));

// paging
void setup_ring3_memory(uint64_t user_code_virt, uint64_t user_stack_virt, const uint8_t *user_code, size_t code_size);
void map_user_page(uint64_t virt_addr, uint64_t phys_addr);
bool protect_user_page(uint64_t virt_addr, bool present, bool write_access,
                       bool execute_access);
bool user_page_mapped(uint64_t virt_addr);
bool unmap_user_page(uint64_t virt_addr);
bool suspend_user_pages(uint64_t *page_table_context);
bool restore_user_pages(const uint64_t *page_table_context);
uint64_t pmm_alloc_zeroed_page(void);
void cleanup_user_pages(void);
extern void to_userland_ring3(uint64_t user_rip, uint64_t user_rsp) __attribute__((noreturn));

// raw exfat functions
#define ATA_DATA         0x1F0
#define ATA_FEATURES     0x1F1
#define ATA_SECTOR_CNT   0x1F2
#define ATA_LBA_LOW      0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HIGH     0x1F5
#define ATA_DRIVE_HEAD   0x1F6
#define ATA_COMMAND      0x1F7
#define ATA_STATUS       0x1F7

#ifndef FS_BASE_LBA
#define FS_BASE_LBA      4096
#endif

#ifndef BLOCK_SIZE
#define BLOCK_SIZE       512
#endif

void        menubar_draw(void);
int         exfat_wipe_and_format(void);
int         exfat_create_node(const char *name, bool is_dir);
int         exfat_write_file(const char *name, const uint8_t *data, uint64_t count);
int         exfat_append_file(const char *name, const uint8_t *data, uint64_t count);
int64_t     exfat_read_file(const char *name, uint8_t *out_buf, uint64_t max_bytes);
int64_t     exfat_read_file_at(const char *name, uint8_t *out_buf, uint64_t offset, uint64_t max_bytes);
int         exfat_delete_node(const char *name);
int         exfat_truncate_last_line(const char *name);
int         exfat_print_directory(int hidden);
int         exfat_list_files(char (*names)[256], size_t capacity);
int         exfat_list_entries(char (*names)[256], size_t capacity);
int         exfat_change_directory(const char *path);
const char* exfat_get_working_dir(void);
int         exfat_mount(void);
int         exfat_get_space_metrics(uint32_t *out_total_clusters, uint32_t *out_free_clusters);

// pci
typedef struct {
    uint64_t address;
    uint64_t size;
    bool is_mmio;
    bool is_64bit;
} pci_bar_t;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
} pci_device_t;

// pci
uint32_t  pci_read_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);
uint16_t  pci_read_16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);
uint8_t   pci_read_8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);

void      pci_write_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t val);

pci_bar_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index);
void      pci_enable_device(uint8_t bus, uint8_t slot, uint8_t func);
void      pci_scan_bus(void);
char*     pci_class_to_name(uint8_t class_code);
bool      parse_slot_string(const char* str, uint8_t* bus, uint8_t* slot, uint8_t* func);

// pcie
void pcie_init_system(uint64_t v_addr, uint8_t start_bus, uint8_t end_bus);
void init_pcie(void);
uint32_t pcie_read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset);
void pcie_write32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset, uint32_t value);
void pcie_scan_bus_system(void (*pcie_callback)(uint8_t b, uint8_t d, uint8_t f, uint16_t ven, uint16_t dev_id));

// nvme stuff
typedef struct {
    uint32_t cap_low;
    uint32_t cap_high;
    uint32_t vs;
    uint32_t intms;
    uint32_t intmc;
    uint32_t cc;
    uint32_t reserved0;
    uint32_t csts;
    uint32_t nssr;
    uint32_t aqa;
    uint64_t asq;
    uint64_t acq;
    uint32_t reserved1;
    uint32_t doorbells[]; 
} __attribute__((packed)) nvme_regs_t;

typedef struct {
    uint8_t  opc;
    uint8_t  fuse_psdt;
    uint16_t cid;
    uint32_t nsid;
    uint64_t reserved0;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_cmd_t;

typedef struct {
    uint32_t cdw0;
    uint32_t reserved;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed)) nvme_cpl_t;

void nvme_init(uintptr_t bar0_mem_base);
void nvme_submit_admin_cmd(uint8_t* raw_cmd, uint8_t* target_cpl);
void nvme_identify_drive(uintptr_t destination_physical_buffer);
void init_nvme_device(uint8_t bus, uint8_t slot, uint8_t func);
void nvme_pci_callback(uint8_t b, uint8_t d, uint8_t f, uint16_t ven, uint16_t dev_id);

// compability with linux syscalls
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
#define LINUX_SYS_EXIT          60
#define LINUX_SYS_UNAME         63
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

#define LINUX_PROT_WRITE  0x2
#define LINUX_PROT_EXEC   0x4
#define LINUX_PROT_NONE   0x0
#define LINUX_MAP_FIXED   0x10
#define LINUX_MAP_ANON    0x20
#define LINUX_MAP_PRIVATE 0x02
#define LINUX_MAP_SHARED  0x01

#define LINUX_EBADF     9
#define LINUX_EIO       5
#define LINUX_EFAULT    14
#define LINUX_EINVAL    22
#define LINUX_EMFILE    24
#define LINUX_ENOENT    2
#define LINUX_ENOMEM    12
#define LINUX_EISDIR    21
#define LINUX_EACCES    13
#define LINUX_ESPIPE    29
#define LINUX_ENOSYS    38
#define LINUX_EOVERFLOW 75
#define LINUX_EFBIG     27

#define USER_IO_LIMIT         65536ULL
#define USER_PATH_LIMIT       256
#define USER_PAGE_SIZE        4096ULL
#define USER_HEAP_BASE        0x0000600000000000ULL
#define USER_MMAP_BASE        0x0000700000000000ULL
#define USER_MMAP_LIMIT       0x00007F0000000000ULL
#define USER_ADDRESS_LIMIT    0x0000800000000000ULL
#define LINUX_FD_LIMIT        32
#define LINUX_FILE_IO_LIMIT   65536ULL
#define LINUX_CLOCK_REALTIME  0
#define LINUX_CLOCK_MONOTONIC 1

// system calls
#define READ_FILE    01
#define WRITE_FILE   02
#define CREATE_FILE  03
#define DELETE_FILE  04
#define CREATE_DIR   05
#define RENAME_FILE  06
#define FILE_EXISTS  07
#define CHANGE_DIR   8
#define DELETE_LAST_LINE 9
#define COPY_FILE    10

#define PRINT        11
#define CLEAR        12
#define INPUT        13
#define GET_KEY      14
#define PRINT_AT     15
#define TEXT_COLOR   16
#define BACKGROUND_COLOR 17
#define MKDIR_RECURSIVE 18
#define GET_TIME     19

// stdlib
#define SHELL        21
#define EXEC         22
#define STAT         23
#define APPEND_FILE  24
#define GET_CWD      25
#define READ_FILE_AT 26
#define MOVE_FILE    27
#define MAP_USER_MEMORY    28
#define PROTECT_USER_MEMORY 29
#define UNMAP_USER_MEMORY  30
#define GET_PID            31
#define GET_UID            32
#define GET_GID            33
#define GET_EUID           34
#define GET_EGID           35
#define GET_UNAME          36
#define RANDOM_BYTES       37
#define GET_USER_NAME      38
#define EXEC_ARGS          39
#define EXEC_ELF           40
#define LIST_DIR           41
#define WRITE_FILE_RAW     42
#define GET_USER           43

// video
#define SET_VIDEO_MODE 44
#define SET_TEXT_MODE  45
#define DRAW_PIXEL     46
#define DRAW_REC       47

#define GET_KEY_DELAY  48
#define ENABLE_STATUS_BAR 49
#define DISABLE_STATUS_BAR 50
#define IS_BAR_ON 51
#define CLEAR_RAW 52
#define EXPAND_USER_PAGES 53

#define SPECIAL 90

#define USER_MEMORY_READ    0x1ULL
#define USER_MEMORY_WRITE   0x2ULL
#define USER_MEMORY_EXEC    0x4ULL
#define USER_MEMORY_MAX_SIZE (16ULL * 1024 * 1024)

#define SYS_EXIT     60

// actual running
int system_run_elf(char* filename, uint64_t size);
void system_run_bin(char *filename, int program_size);
void system_exec(char* path);

// random utilities
int util_hexdump(char* filename);
int util_calc(char* string);
int kernel_package(int argc, char **argv);
char* get_user(void);
void sleep_ms(uint64_t miliseconds);

#endif
