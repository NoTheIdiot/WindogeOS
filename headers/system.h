#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <bool.h>
#include <stddef.h>

int system_dogeshell_ex(char* command);
void system_dogeshell();
void system_bash();

void system_fetch();
extern char* windoge_version;
extern char current_user[64];
extern uint32_t old;
extern char* dogeshell_version;
extern char computer_name[64];

void system_editor(char* filename);
void system_settings();

uint64_t get_ram_end_address(void);
int system_create_user(char* name, char* password, int permission_id);
int system_verify_user(const char* name, char* password);
int system_can_access_path(const char* username, const char* target_path);
void system_run_exec(char *filename, int program_size);

// paging
void setup_ring3_memory(uint64_t user_code_virt, uint64_t user_stack_virt, const uint8_t *user_code, size_t code_size);
void map_user_page(uint64_t virt_addr, uint64_t phys_addr);
uint64_t pmm_alloc_zeroed_page(void);
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
int         exfat_delete_node(const char *name);
int         exfat_truncate_last_line(const char *name);
int         exfat_print_directory(int hidden);
int         exfat_change_directory(const char *path);
const char* exfat_get_working_dir(void);
int         exfat_mount(void);

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


uint32_t  pci_read_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);
uint16_t  pci_read_16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);
uint8_t   pci_read_8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);

void      pci_write_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t val);

pci_bar_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index);
void      pci_enable_device(uint8_t bus, uint8_t slot, uint8_t func);
void      pci_scan_bus(void);

// system calls
#define FS_READ	01
#define FS_WRITE 02
#define FS_EXISTS 03
#define FS_CREATE 04
#define FS_MKDIR 05
#define FS_DELETE 06 
#define FS_RENAME 07
#define FS_READ_RAW 08

#define DOGEIO_PRINT 11
#define DOGEIO_CLEAR 12
#define DOGEIO_INPUT 13
#define DOGEIO_COLOR 14
#define DOGEIO_BACKGROUND 15
#define DOGEIO_CLEAR_RAW 16

#define SYS_EXIT 0

#endif
