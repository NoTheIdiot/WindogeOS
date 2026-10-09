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

void system_fetch();
extern char* windoge_version;
extern char current_user[64];
extern uint32_t old;
extern char* dogeshell_version;
extern char computer_name[64];

void system_editor(char* filename);

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
#define PRINTLN      12
#define CLEAR        13
#define INPUT        14
#define GET_KEY      15
#define PRINT_AT     16
#define TEXT_COLOR   17
#define BACKGROUND_COLOR 18
#define MKDIR_RECURSIVE 19
#define GET_TIME     20

// stdlib
#define SHELL        21
#define EXEC         22
#define STAT         23
#define APPEND_FILE  24
#define GET_CWD      25

#define SYS_EXIT     60

// actual running
int system_run_elf(char* filename, uint64_t size);
void system_run_bin(char *filename, int program_size);

// random utilities
int util_hexdump(char* filename);
int util_calc(char* string);

#endif
