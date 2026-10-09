#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <system.h>
#include <basicutil.h>
#include <boot/limine.h>

#define ELF_CLASS_64 2
#define ELF_DATA_LSB 1
#define ELF_VERSION_CURRENT 1
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_X86_64 62
#define PT_LOAD 1
#define PF_X 1
#define PAGE_SIZE 4096ULL
#define USER_ADDRESS_LIMIT 0x0000800000000000ULL
#define USER_STACK_VADDR 0x00007FFFFFFFF000ULL
#define MAX_ELF_FILE_SIZE (16ULL * 1024 * 1024)
#define MAX_ELF_MEMORY_SIZE (64ULL * 1024 * 1024)
#define MAX_ELF_PROGRAM_HEADERS 64

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

static uint64_t align_down_page(uint64_t value) {
    return value & ~(PAGE_SIZE - 1);
}

static uint64_t align_up_page(uint64_t value) {
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

int system_run_elf_impl(char *filename, uint64_t size) {
    if (filename == NULL || filename[0] == '\0' ||
        size < sizeof(elf64_ehdr) || size > MAX_ELF_FILE_SIZE) {
        log("sys (elf): invalid name or file size");
        return -1;
    }

    if (!fs_exists(filename)) {
        log("sys (elf): file does not exist");
        return -1;
    }

    if (hhdm_request.response == NULL) {
        log("sys (elf): HHDM response is unavailable");
        return -2;
    }

    cleanup_user_pages();

    int result = -3;
    elf64_ehdr ehdr;
    int read_bytes = fs_read_raw_at(filename, (uint8_t *)&ehdr, 0, sizeof(ehdr));
    if (read_bytes != (int)sizeof(ehdr)) {
        result = -4;
        goto failure;
    }

    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F' ||
        ehdr.e_ident[4] != ELF_CLASS_64 ||
        ehdr.e_ident[5] != ELF_DATA_LSB ||
        ehdr.e_ident[6] != ELF_VERSION_CURRENT ||
        ehdr.e_type != ELF_TYPE_EXEC ||
        ehdr.e_machine != ELF_MACHINE_X86_64 ||
        ehdr.e_version != ELF_VERSION_CURRENT ||
        ehdr.e_ehsize != sizeof(elf64_ehdr) ||
        ehdr.e_phentsize != sizeof(elf64_phdr) ||
        ehdr.e_phnum == 0 || ehdr.e_phnum > MAX_ELF_PROGRAM_HEADERS) {
        result = -5;
        goto failure;
    }

    uint64_t phdr_bytes = (uint64_t)ehdr.e_phnum * ehdr.e_phentsize;
    if (ehdr.e_phoff > size || phdr_bytes > size - ehdr.e_phoff) {
        result = -5;
        goto failure;
    }

    elf64_phdr phdrs[MAX_ELF_PROGRAM_HEADERS];
    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        uint64_t phdr_offset = ehdr.e_phoff + (uint64_t)i * ehdr.e_phentsize;
        read_bytes = fs_read_raw_at(filename, (uint8_t *)&phdrs[i],
                                    phdr_offset, sizeof(elf64_phdr));
        if (read_bytes != (int)sizeof(elf64_phdr)) {
            result = -5;
            goto failure;
        }
    }

    uint64_t total_pages = 0;
    bool entry_is_executable = false;

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        elf64_phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) {
            continue;
        }

        if (phdr->p_filesz > phdr->p_memsz ||
            phdr->p_offset > size ||
            phdr->p_filesz > size - phdr->p_offset ||
            phdr->p_vaddr < PAGE_SIZE ||
            phdr->p_vaddr >= USER_STACK_VADDR ||
            phdr->p_memsz > USER_STACK_VADDR - phdr->p_vaddr) {
            result = -5;
            goto failure;
        }

        if (phdr->p_align > 1 &&
            ((phdr->p_align & (phdr->p_align - 1)) != 0 ||
             (phdr->p_vaddr & (phdr->p_align - 1)) !=
             (phdr->p_offset & (phdr->p_align - 1)))) {
            result = -5;
            goto failure;
        }

        uint64_t seg_end = phdr->p_vaddr + phdr->p_memsz;
        uint64_t page_start = align_down_page(phdr->p_vaddr);
        uint64_t page_end = align_up_page(seg_end);
        uint64_t segment_pages = (page_end - page_start) / PAGE_SIZE;

        if (segment_pages > MAX_ELF_MEMORY_SIZE / PAGE_SIZE - total_pages) {
            result = -5;
            goto failure;
        }
        total_pages += segment_pages;

        uint64_t file_end = phdr->p_vaddr + phdr->p_filesz;
        if ((phdr->p_flags & PF_X) &&
            ehdr.e_entry >= phdr->p_vaddr && ehdr.e_entry < seg_end) {
            entry_is_executable = true;
        }

        for (uint16_t j = 0; j < i; j++) {
            elf64_phdr *other = &phdrs[j];
            if (other->p_type != PT_LOAD || other->p_memsz == 0) {
                continue;
            }
            uint64_t other_start = align_down_page(other->p_vaddr);
            uint64_t other_end = align_up_page(other->p_vaddr + other->p_memsz);
            if (page_start < other_end && other_start < page_end) {
                result = -5;
                goto failure;
            }
        }

        for (uint64_t page_vaddr = page_start; page_vaddr < page_end;
             page_vaddr += PAGE_SIZE) {
            uint64_t page_phys = pmm_alloc_zeroed_page();
            if (page_phys == 0) {
                result = -3;
                goto failure;
            }

            map_user_page(page_vaddr, page_phys);

            uint64_t copy_start = page_vaddr > phdr->p_vaddr ?
                                  page_vaddr : phdr->p_vaddr;
            uint64_t page_limit = page_vaddr + PAGE_SIZE;
            uint64_t copy_end = page_limit < file_end ? page_limit : file_end;
            if (copy_start < copy_end) {
                uint64_t file_offset = phdr->p_offset + (copy_start - phdr->p_vaddr);
                uint64_t copy_size = copy_end - copy_start;
                read_bytes = fs_read_raw_at(
                    filename,
                    (uint8_t *)(page_phys + hhdm_request.response->offset +
                                (copy_start - page_vaddr)),
                    file_offset,
                    copy_size);
                if (read_bytes < 0 || (uint64_t)read_bytes != copy_size) {
                    result = -4;
                    goto failure;
                }
            }
        }
    }

    if (total_pages == 0 || !entry_is_executable ||
        ehdr.e_entry >= USER_ADDRESS_LIMIT) {
        result = -5;
        goto failure;
    }

    uint64_t stack_phys = pmm_alloc_zeroed_page();
    if (stack_phys == 0) {
        result = -3;
        goto failure;
    }
    map_user_page(USER_STACK_VADDR, stack_phys);

    to_userland_ring3(ehdr.e_entry, USER_STACK_VADDR + PAGE_SIZE - 8);

failure:
    cleanup_user_pages();
    return result;
}
