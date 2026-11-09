#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <dogeio.h>
#include <system.h>
#include <limine.h>

#define ELF_CLASS_64        2
#define ELF_DATA_LSB        1
#define ELF_VERSION_CURRENT 1
#define ELF_TYPE_EXEC       2
#define ELF_MACHINE_X86_64  62

#define PT_LOAD             1

// Correct stack definitions: Stack starts high and grows downward
#define USER_STACK_TOP        0x00007FFFFFFFF000ULL
#define USER_STACK_PAGE_COUNT 4
#define USER_STACK_SIZE       (PAGE_SIZE * USER_STACK_PAGE_COUNT)
#define USER_STACK_BASE       (USER_STACK_TOP - USER_STACK_SIZE)

typedef struct __attribute__((packed)) {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr;

typedef struct __attribute__((packed)) {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr;

extern volatile struct limine_hhdm_request hhdm_request;

extern void to_userland_ring3_args(
    uint64_t entry,
    uint64_t stack,
    uint64_t arg0,
    uint64_t arg1
);

static uint64_t align_down_page(uint64_t value)
{
    return value & ~(PAGE_SIZE - 1);
}

static uint64_t align_up_page(uint64_t value)
{
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

int run_elf(char *filename, uint64_t size)
{
    if (!filename || size < sizeof(elf64_ehdr))
        return -1;

    elf64_ehdr ehdr;

    if (fs_read_raw_at(
            filename,
            (uint8_t *)&ehdr,
            0,
            sizeof(ehdr)) != sizeof(ehdr))
    {
        return -2;
    }

    if (ehdr.e_ident[0] != 0x7F ||
        ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' ||
        ehdr.e_ident[3] != 'F' ||
        ehdr.e_ident[4] != ELF_CLASS_64 ||
        ehdr.e_ident[5] != ELF_DATA_LSB ||
        ehdr.e_type != ELF_TYPE_EXEC ||
        ehdr.e_machine != ELF_MACHINE_X86_64)
    {
        return -3;
    }

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {

        elf64_phdr phdr;

        uint64_t phdr_offset =
            ehdr.e_phoff +
            ((uint64_t)i * ehdr.e_phentsize);

        if (fs_read_raw_at(
                filename,
                (uint8_t *)&phdr,
                phdr_offset,
                sizeof(phdr)) != sizeof(phdr))
        {
            return -4;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0)
            continue;

        uint64_t page_start =
            align_down_page(phdr.p_vaddr);

        uint64_t page_end =
            align_up_page(phdr.p_vaddr + phdr.p_memsz);

        uint64_t file_end =
            phdr.p_vaddr + phdr.p_filesz;

        for (uint64_t page_vaddr = page_start;
             page_vaddr < page_end;
             page_vaddr += PAGE_SIZE)
        {
            uint64_t page_phys =
                pmm_alloc_zeroed_page();

            if (!page_phys)
                return -5;

            map_user_page(page_vaddr, page_phys);

            uint64_t copy_start =
                (page_vaddr > phdr.p_vaddr)
                    ? page_vaddr
                    : phdr.p_vaddr;

            uint64_t copy_end =
                ((page_vaddr + PAGE_SIZE) < file_end)
                    ? (page_vaddr + PAGE_SIZE)
                    : file_end;

            if (copy_start < copy_end) {

                uint64_t file_offset =
                    phdr.p_offset +
                    (copy_start - phdr.p_vaddr);

                uint64_t copy_size =
                    copy_end - copy_start;

                uint8_t *dest =
                    (uint8_t *)(
                        page_phys +
                        hhdm_request.response->offset +
                        (copy_start - page_vaddr));

                if (fs_read_raw_at(
                        filename,
                        dest,
                        file_offset,
                        copy_size) != (int)copy_size)
                {
                    return -6;
                }
            }
        }
    }

    // Allocate user stack pages downward from USER_STACK_TOP
    for (uint64_t i = 0;
         i < USER_STACK_PAGE_COUNT;
         i++)
    {
        uint64_t page_phys =
            pmm_alloc_zeroed_page();

        if (!page_phys)
            return -7;

        map_user_page(
            USER_STACK_BASE + (i * PAGE_SIZE),
            page_phys);
    }

    uint64_t stack_top = USER_STACK_TOP & ~0xFULL;

    char num_buf[32];
    println("[ELF] Loading userland program...");
    
    print("      Entry: ");
    uint64_to_str(ehdr.e_entry, num_buf);
    println(num_buf);
    
    print("      Stack: ");
    uint64_to_str(stack_top, num_buf);
    println(num_buf);

    to_userland_ring3_args(
        ehdr.e_entry,
        stack_top,
        0,  /* argc */
        0   /* argv */
    );

    for (;;)
        __asm__ volatile ("hlt");

    return 0;
}