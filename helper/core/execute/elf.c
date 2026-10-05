#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <system.h>
#include <basicutil.h>
#include <boot/limine.h>

#define ELF_MAGIC 0x464C457F

#define PT_LOAD        1
#define PT_X           1
#define PF_W           2
#define PF_R           4

typedef struct __attribute__((packed))  {
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
} elf64_edhr;

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

int system_run_elf(char* filename, uint64_t size) {
    if (filename[0] == '\0' || size <= 0) {
        log("sys (elf): invalid name or size for running");
        return -1;
    }

    if (!fs_exists(filename)) {
        log("sys (elf): file doens't exist");
        return -1;
    }

    if (hhdm_request.response == NULL) {
        log("sys (elf): hhdm request is null (doesn't respond)");
        return -2;
    }

    cleanup_user_pages();

    uint64_t hhdm_offset = hhdm_request.response->offset;

    uint64_t file_pages = (size + 4095) / 4096;
    uint64_t file_phys_start = 0;
    
    for (uint64_t i = 0; i < file_pages; i++) {
        uint64_t p = pmm_alloc_zeroed_page();
        if (!p) return -3;
        if (i == 0) file_phys_start = p;
    }

    uint8_t *file_buf = (uint8_t *)(file_phys_start + hhdm_offset);
    int read_bytes = fs_read_raw(filename, file_buf, size);
    if (read_bytes < (int)sizeof(elf64_edhr)) {
        return -4;
    }

    elf64_edhr *ehdr = (elf64_edhr *)file_buf;

    if (*(uint32_t *)ehdr->e_ident != ELF_MAGIC) {
        return -5;
    }

    elf64_phdr *phdrs = (elf64_phdr *)(file_buf + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        elf64_phdr phdr = phdrs[i];

        if (phdr.p_type == PT_LOAD) {
            uint64_t page_count = (phdr.p_memsz + 4095) / 4096;

            for (uint64_t p = 0; p < page_count; p++) {
                uint64_t segment_phys = pmm_alloc_zeroed_page();
                uint64_t page_offset = p * 4096;

                if (page_offset < phdr.p_filesz) {
                    size_t bytes_to_read = phdr.p_filesz - page_offset;
                    if (bytes_to_read > 4096) {
                        bytes_to_read = 4096;
                    }
                    memcpy((void *)(segment_phys + hhdm_offset), file_buf + phdr.p_offset + page_offset, bytes_to_read);
                }

                map_user_page(phdr.p_vaddr + page_offset, segment_phys);
            }
        }
    }

    uint64_t stack_phys = pmm_alloc_zeroed_page();
    uint64_t stack_vaddr = 0x7FFFFFFFF000;
    map_user_page(stack_vaddr, stack_phys);

    uint64_t user_rsp = stack_vaddr + 4096 - 8;
    uint64_t entry_point = ehdr->e_entry;

    to_userland_ring3(entry_point, user_rsp);

    return 0;
}
