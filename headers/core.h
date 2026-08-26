#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

typedef struct {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) tss_descriptor_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct {
    gdt_entry_t      null_desc;   // 0x00
    gdt_entry_t      kernel_code; // 0x08
    gdt_entry_t      kernel_data; // 0x10
    gdt_entry_t      user_data;   // 0x18
    gdt_entry_t      user_code;   // 0x20
    tss_descriptor_t tss_desc;    // 0x28
} __attribute__((packed, aligned(4096))) gdt_table_t;

void init_gdt(void);
extern void gdt_flush(gdt_ptr_t *ptr);

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector_num;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_frame_t;

void init_idt(void);
extern void idt_load(idt_ptr_t *ptr);
void init_tss(void);

#define DOGEIO_TEXT_PUTCHAR           1
#define DOGEIO_TEXT_CLEAR             2
#define DOGEIO_TEXT_PRINTCHAR         3
#define DOGEIO_TEXT_PRINT             4
#define DOGEIO_TEXT_PRINTLN           5
#define DOGEIO_TEXT_PRINT_AT          6
#define DOGEIO_TEXT_INPUT             7
#define DOGEIO_TEXT_COLOR_CHANGE      8
#define DOGEIO_TEXT_BACKGROUND_CHANGE 9
#define DOGEIO_TEXT_CLEAR_RAW         10

#define DOGEIO_FS_FORMAT              11
#define DOGEIO_FS_CREATE              12
#define DOGEIO_FS_MKDIR               13
#define DOGEIO_FS_EXISTS              14
#define DOGEIO_FS_DELETE              15
#define DOGEIO_FS_DELETE_LAST_LINE    16
#define DOGEIO_FS_READ                17
#define DOGEIO_FS_WRITE               18
#define DOGEIO_FS_LIST_DIR            19
#define DOGEIO_FS_RENAME              20
#define DOGEIO_FS_COPY                21
#define DOGEIO_FS_CHDIR               22
#define DOGEIO_FS_DIRNAME             23
#define DOGEIO_FS_MOUNT               24

#define DOGEIO_EXEC_FLAT_BINARY       25

extern void *kernel_stack_top;
extern void *user_rsp_scratch;

void syscall_entry(void);
uint64_t c_syscall_handler(uint64_t sys_id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);
void core_to_user(uint64_t user_pml4_phys, void *user_entry, void *user_stack_top);

static inline uint64_t core_syscall(uint64_t sys_id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    uint64_t result;
    register uint64_t r10 __asm__("r10") = arg4;

    __asm__ volatile (
        "syscall\n\t"
        : "=a"(result)
        : "a"(sys_id), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );

    return result;
}

void init_syscall(void);
void init_user_space(void);
uint64_t pmm_alloc_block(void);

#endif