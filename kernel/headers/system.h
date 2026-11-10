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

// ata
struct __attribute__((packed)) ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
};

struct __attribute__((packed)) ext2_bgd {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
};

struct __attribute__((packed)) ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t  i_osd2[12];
};

struct __attribute__((packed)) ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
};

int ahci_exec_cmd(uint32_t port_num, uint64_t lba, uint32_t count, uint8_t write, uintptr_t buf_phys);
void ahci_sata_init(uintptr_t ecam_base_addr);
void test_ata_persistence(void);
uint8_t ata_current_drive(void);
void ata_probe_devices(void);
int ata_drive_present(uint8_t drive);
uint64_t ata_drive_sector_count(uint8_t drive);
int ata_read_sector(uint8_t drive, uint64_t lba, uint8_t *buffer);
int ata_write_sector(uint8_t drive, uint64_t lba, const uint8_t *buffer);

// raw exfat functions
#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_ROOT_INODE 2

int ext2_mount(uint8_t drive);
int ext2_read_inode(uint32_t inode_num, struct ext2_inode *out_inode);
int ext2_list_root_dir(void);
int ext2_write_inode(uint32_t inode_num, const struct ext2_inode *in_inode);
uint32_t ext2_lookup(const char *filename);
int ext2_create_file(const char *filename, uint16_t mode);
int ext2_list(fs_t *out_list, int max_entries);

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