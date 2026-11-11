#ifndef SYSTEM_H
#define SYSTEM_H

#include "../helper/io/io.h"
#include "../helper/io/asm.h"
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <bool.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define PAGE_LARGE    (1ULL << 7)
#define PAGE_NO_EXECUTE (1ULL << 63)
#define PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define MSR_IA32_EFER 0xC0000080
#define EFER_NXE (1ULL << 11)
#define DATA_AMOUNT 8192
#define PAGE_USER_FLAGS (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)

#define ATA_DATA        0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DF   0x20
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_BSY  0x80
#define ATA_TIMEOUT     1000000000U

#define ATA_DATA         0x1F0
#define ATA_FEATURES     0x1F1
#define ATA_SECTOR_CNT   0x1F2
#define ATA_LBA_LOW      0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HIGH     0x1F5
#define ATA_DRIVE_HEAD   0x1F6
#define ATA_COMMAND      0x1F7
#define ATA_STATUS       0x1F7
#define ATA_DRIVE_COUNT  2

#ifndef BLOCK_SIZE
#define BLOCK_SIZE       512
#endif
#define ATA_PRIMARY_MASTER     0
#define ATA_PRIMARY_SLAVE      1
#define ATA_SECONDARY_MASTER   2
#define ATA_SECONDARY_SLAVE    3

// init functions
void init_gdt_tss(void);
void init_idt(void);
void disable_legacy_pic(void);
void init_apic(void);
void serial_init(void);
void init_rtc_time(void);
void init_syscalls(void);
void init_pci_ecam(uintptr_t rsdp_phys_addr);
void init_usb_keyboard(uint8_t device_address);
bool pci_find_ahci(uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func, uint64_t *out_bar5_phys);

// paging
#define PAGE_SIZE           4096ULL
#define PAGE_PRESENT        (1ULL << 0)
#define PAGE_WRITABLE       (1ULL << 1)
#define PAGE_USER           (1ULL << 2)
#define PAGE_WRITE_THROUGH  (1ULL << 3)
#define PAGE_CACHE_DISABLE  (1ULL << 4)
#define PAGE_LARGE          (1ULL << 7)
#define PAGE_NO_EXECUTE     (1ULL << 63)
#define PAGE_ADDRESS_MASK   0x000FFFFFFFFFF000ULL

#define MSR_IA32_EFER       0xC0000080
#define EFER_NXE            (1ULL << 11)
#define DATA_AMOUNT         8192
#define PAGE_USER_FLAGS     (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)

typedef struct {
    uint64_t start;
    uint64_t end;
} memory_region_t;

typedef struct {
    memory_region_t *regions;
    uint64_t region_count;
    uint64_t stack_base;
    uint64_t stack_pages;
} program_memory_t;

void init_vmm(void);
void cleanup_program_pages(const program_memory_t *program);
uint64_t pmm_alloc_zeroed_page(void);
size_t pmm_get_free_page_count(void);
void cleanup_pages(void);
uint64_t create_user_pml4(void);
void switch_user_pml4(uint64_t pml4_phys);
void destroy_user_pml4(uint64_t pml4_phys);
bool suspend_user_pages(uint64_t *page_table_context);
bool restore_user_pages(const uint64_t *page_table_context);
uintptr_t map_kernel_mmio_page(uintptr_t virt_addr, uint64_t phys_addr);
void map_user_page(uint64_t virt_addr, uint64_t phys_addr);
bool protect_user_page(uint64_t virt_addr, bool present, bool write_access, bool execute_access);
bool user_page_mapped(uint64_t virt_addr);
bool unmap_user_page(uint64_t virt_addr);
void setup_ring3_memory(uint64_t user_code_virt, uint64_t user_stack_virt, const uint8_t *user_code, size_t code_size);


int ahci_exec_cmd(uint32_t port_num, uint64_t lba, uint32_t count, uint8_t write, uintptr_t buf_phys);
void ahci_sata_init(uintptr_t ecam_base_addr);
void test_ata_persistence(void);
uint8_t ata_current_drive(void);
void ata_probe_devices(void);
int ata_drive_present(uint8_t drive);
uint64_t ata_drive_sector_count(uint8_t drive);
int ata_read_sector(uint8_t drive, uint64_t lba, uint8_t *buffer);
int ata_write_sector(uint8_t drive, uint64_t lba, const uint8_t *buffer);

typedef struct __attribute__((packed)) {
    uint8_t jump[3];        // 0xEB, 0x76, and 0x90
    char fs_name[8];        // is "EXFAT   "
    uint8_t zeros[53];      // /dev/zero that shi
    uint64_t offset;        // offset
    uint64_t volume;        // volume length
    uint32_t foffset;       // fat offset, from FAT origin
    uint32_t flength;       // fat length, size of a FAT in unit of 
                            // sector
    uint32_t choffset;      // cluster heap offset origin from the top 
                            // of the volume.
    uint32_t ccount;        // cluster count, about of clusters on drive.
    uint32_t fcord;         // first cluster at root
    uint32_t serial;        // volume serial number
    uint16_t fs_revision;   // file system revision
    uint16_t volume_flags;  // bit 0 is active FAT, bit 1 is volume dirty,
                            // bit 2 is failure, rest are zeros
    uint8_t sector_shift;   // shifting up
    uint8_t sectors_pch;    // sectirs per cluster shift
    uint8_t fat_num;        // number of fat
    uint8_t drive;          // yo what drive are we
    uint8_t percent_used;   // percnt of drive used, 0xFF is invalid (-1)
    uint8_t reserved[7];    // guess what, more zeros.
    uint8_t code[390];      // thats alot of empty space
    uint16_t sig;           // signature
} exfat_start_sector;

typedef struct __attribute__((packed)) {
    uint8_t entry_type; 
    uint8_t scount;
    uint16_t checksum;
    uint16_t attribute;
    uint16_t reserved1;
    uint32_t time_stamp;
    uint32_t last_modified;
    uint32_t last_accessed;
    uint8_t increment;
    uint8_t last_modified_increment;
    uint8_t TZoffset;
    uint8_t last_modified_TZoffset;
    uint8_t last_accessed_TZoffset;
    uint8_t reserved2[7];
} file_entry;

typedef struct __attribute__((packed)) {
    uint8_t entry_type;
    uint8_t secondary_flags;
    uint8_t reserved1;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t reserved2;
    uint64_t valid_data_length;
    uint32_t reserved3;
    uint32_t first_cluster;
    uint64_t data_length;
} stream_entry;

typedef struct __attribute__((packed)) {
    uint8_t entry_type;
    uint8_t secondary_flags;
    char filename[30];
} file_name_entry;

typedef struct {
    uint64_t poffset_bytes;

    uint32_t bytes_per_sector;      // guess what idiots, it's 512bytes.
    uint32_t sectors_per_cluster;   // how many sectors are in a single cluster
    uint32_t bytes_per_cluster;     // how about bytes are in a cluster?

    uint64_t offset_bytes;          // the offset from each fat (bad name microsoft)
    uint64_t heap_offset_bytes;
    uint32_t root_cluster;          // the cluster at root
    uint32_t cluster_count;         // mate, how many clusters are there?

    // second stage, where you get your hopes up then fall 
    // into depression, Also known has bitmaps and upcases.
    uint32_t bitmap_cluster;
    uint64_t bitmap_size;
    uint32_t upcase_cluster;
    uint64_t upcase_size;
} mount_t;

// idk what are bitmaps
typedef struct __attribute__((packed)) {
    uint8_t entry_type;
    uint8_t flags;
    uint8_t reserved[18];           // thought mainstream operating systems aren't bloated
                                    // enough? MORE RESERVED ZEROS :D
    uint32_t first_cluster;         // first cluster in a bitmap
    uint64_t data_length;           // size of bitmap in bytes
} bitmap;

// upcases
typedef struct __attribute__((packed)) {
    uint8_t entry_type;             // is 0x82
    uint8_t reserved1;              // guess what, more zeros.
    uint32_t checksum;              // checksum, idk the magic.
    uint8_t reserved[12];           // OH GOD MORE ZEROS HELP ME
    uint32_t first_cluster;         // first cluster of the upcase table
    uint64_t size;                  // size of table;
} upcase;

int mount(mount_t *vol, uint8_t *cluster_buf);

// register typedef
typedef struct regstate {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} registers_t;

// pci / pcie and IO
void pcie_init_system(uint64_t v_addr, uint8_t start_bus, uint8_t end_bus);
void init_pcie(void);
uint32_t pcie_read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset);
void pcie_write32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset, uint32_t value);

// usb 1.0/1.1 (uhci)
bool uhci_init_keyboard_endpoint(uint8_t device_address, uint8_t endpoint_num);
bool uhci_find_controller(uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func, uint16_t *out_bar4);
void uhci_init_driver(void);
void uhci_poll_ports(void);
void uhci_keyboard_interrupt_handler(void);

// apic
void apic_eoi(void);

// power state functions
void reboot(void);
void shutdown(void);

#endif