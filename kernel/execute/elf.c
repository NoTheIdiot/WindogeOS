#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <bool.h>
#include <io.h>

extern void to_userland_ring3(uint64_t entry, uint64_t stack_top, int argc, uint64_t argv_user);

#define ELF_SCRATCH_VBASE 0xFFFF900000000000ULL
#define MAX_SAVED_PAGES   256
#define MAX_ARGS          64

#ifndef USER_STACK_PAGES
#define USER_STACK_PAGES  8
#endif

#ifndef USER_STACK_TOP
#define USER_STACK_TOP    0x00007FFFFFF00000ULL
#endif

#ifndef USER_ADDR_MAX
#define USER_ADDR_MAX     0x0000800000000000ULL
#endif

typedef struct {
    uint64_t vaddr;
    uint64_t phys;
    uint8_t  data[PAGE_SIZE];
} saved_page_t;

static saved_page_t g_saved_pages[MAX_SAVED_PAGES];
static size_t g_saved_page_count = 0;
static size_t g_current_scratch_pages = 0;

const char *elf_error_messages[] = {
    "ELF_OK",
    "ELF_INVALID_MAGIC",
    "ELF_UNSUPPORTED_CLASS",
    "ELF_UNSUPPORTED_DATA_ENCODING",
    "ELF_UNSUPPORTED_VERSION",
    "ELF_UNSUPPORTED_TYPE",
    "ELF_UNSUPPORTED_MACHINE",
    "ELF_INVALID_PROGRAM_HEADER_OFFSET",
    "ELF_INVALID_SECTION_HEADER_OFFSET",
    "ELF_INVALID_PROGRAM_HEADER_SIZE",
    "ELF_INVALID_SECTION_HEADER_SIZE",
    "ELF_INVALID_PROGRAM_HEADER_COUNT",
    "ELF_INVALID_SECTION_HEADER_COUNT",
    "ELF_INVALID_SECTION_NAME_STRING_TABLE_INDEX",
    "ELF_UNSUPPORTED_OSABI"
};

static void zero_mem(void* dst, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < n; i++) d[i] = 0;
}

static void copy_mem(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

static size_t str_len(const char* s) {
    size_t len = 0;
    if (!s) return 0;
    while (s[len] != '\0') len++;
    return len;
}

static inline void flush_tlb_page(uint64_t vaddr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
}

static int add_overflow_u64(uint64_t a, uint64_t b, uint64_t* out) {
    if (UINT64_MAX - a < b) {
        return 1;
    }
    *out = a + b;
    return 0;
}

static void backup_range(uint64_t start, uint64_t end) {
    uintptr_t tmp_va = ELF_SCRATCH_VBASE + ((g_current_scratch_pages + 16ULL) * PAGE_SIZE);

    for (uint64_t vaddr = start; vaddr < end; vaddr += PAGE_SIZE) {
        if (g_saved_page_count >= MAX_SAVED_PAGES) {
            sprintf("[ELF] WARNING: backup page limit (%d) reached at 0x%llx\n",
                    MAX_SAVED_PAGES, (unsigned long long)vaddr);
            break;
        }

        uintptr_t phys = unmap_page(vaddr);
        flush_tlb_page(vaddr);
        if (!phys) {
            continue;
        }

        saved_page_t* slot = &g_saved_pages[g_saved_page_count++];
        slot->vaddr = vaddr;
        slot->phys = (uint64_t)phys;

        if (map_page(tmp_va, phys, PAGE_PRESENT | PAGE_WRITABLE)) {
            flush_tlb_page(tmp_va);
            copy_mem(slot->data, (const void*)tmp_va, PAGE_SIZE);
            unmap_page(tmp_va);
            flush_tlb_page(tmp_va);
        } else {
            zero_mem(slot->data, PAGE_SIZE);
        }
    }
}

static void restore_backed_up_pages(void) {
    for (size_t i = 0; i < g_saved_page_count; i++) {
        saved_page_t* slot = &g_saved_pages[i];
        uintptr_t existing = unmap_page(slot->vaddr);
        flush_tlb_page(slot->vaddr);
        if (existing) {
            free_page(existing);
        }

        uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        if (map_page(slot->vaddr, slot->phys, flags)) {
            flush_tlb_page(slot->vaddr);
            copy_mem((void*)slot->vaddr, slot->data, PAGE_SIZE);
        } else {
            sprintf("[ELF] ERROR: failed to restore backed-up page at 0x%llx\n",
                    (unsigned long long)slot->vaddr);
            free_page(slot->phys);
        }
    }
    g_saved_page_count = 0;
}

static void unmap_range(uint64_t start, uint64_t end) {
    for (uint64_t vaddr = start; vaddr < end; vaddr += PAGE_SIZE) {
        uintptr_t phys = unmap_page(vaddr);
        flush_tlb_page(vaddr);
        if (phys) {
            free_page(phys);
        }
    }
}

void program_unload(elf_process_t* proc) {
    if (!proc) return;

    if (proc->max_vaddr > proc->min_vaddr) {
        unmap_range(proc->min_vaddr, proc->max_vaddr);
    }

    if (proc->stack_top != 0) {
        uint64_t stack_bottom = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
        unmap_range(stack_bottom, USER_STACK_TOP);
    }

    restore_backed_up_pages();
    zero_mem(proc, sizeof(elf_process_t));
}

static int setup_user_stack_args(uint64_t stack_bottom,
                                 uint64_t stack_top,
                                 int argc,
                                 char** argv,
                                 uint64_t* out_sp,
                                 uint64_t* out_argv) {
    if (argc < 0 || argc > MAX_ARGS) {
        return -1;
    }
    if (argc > 0 && !argv) {
        return -1;
    }

    uint64_t sp = stack_top;
    uint64_t arg_ptrs[MAX_ARGS + 1];
    zero_mem(arg_ptrs, sizeof(arg_ptrs));

    if (sp < stack_bottom + 16) {
        return -1;
    }
    sp -= 16;

    for (int i = argc - 1; i >= 0; i--) {
        const char* s = argv[i] ? argv[i] : "";
        size_t len = str_len(s) + 1;

        if (sp < stack_bottom + len) {
            return -1;
        }
        sp -= len;
        copy_mem((void*)(uintptr_t)sp, s, len);
        arg_ptrs[i] = sp;
        sprintf("[ELF]   argv[%d] -> 0x%llx \"%s\"\n", i, (unsigned long long)sp, s);
    }

    sp &= ~0x7ULL;

    size_t ptr_count = (size_t)argc + 1;
    size_t ptr_bytes = ptr_count * sizeof(uint64_t);

    /* Layout: [argc] [argv[0]..argv[argc-1]] [NULL] */
    size_t total_frame_bytes = ptr_bytes + sizeof(uint64_t);
    if (sp < stack_bottom + total_frame_bytes + 16) {
        return -1;
    }

    uint64_t candidate_sp = (sp - total_frame_bytes) & ~0xFULL;
    if (candidate_sp < stack_bottom) {
        return -1;
    }
    sp = candidate_sp;

    uint64_t* frame = (uint64_t*)(uintptr_t)sp;
    frame[0] = (uint64_t)argc;
    for (int i = 0; i < argc; i++) {
        frame[1 + i] = arg_ptrs[i];
    }
    frame[1 + argc] = 0;

    *out_sp = sp;
    *out_argv = sp + sizeof(uint64_t);
    return 0;
}

status validate(const uint8_t* file_buf, size_t file_size, uint8_t* out) {
    if (!file_buf || file_size < sizeof(Elf64_Ehdr)) {
        return ELF_INVALID_MAGIC;
    }

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)file_buf;

    if (ehdr->e_ident[0] != magic[0] ||
        ehdr->e_ident[1] != magic[1] ||
        ehdr->e_ident[2] != magic[2] ||
        ehdr->e_ident[3] != magic[3]) {
        return ELF_INVALID_MAGIC;
    }

    if (ehdr->e_ident[4] != CLASS64)     return ELF_UNSUPPORTED_CLASS;
    if (ehdr->e_ident[5] != ELFDATA2LSB) return ELF_UNSUPPORTED_DATA_ENCODING;
    if (ehdr->e_ident[6] != EV_CURRENT)  return ELF_UNSUPPORTED_VERSION;

    uint8_t osabi = ehdr->e_ident[7];
    if (osabi != WINDOGE_MARK && osabi != 0x00 && osabi != 0x03) {
        return ELF_UNSUPPORTED_OSABI;
    }
    if (out) {
        *out = osabi;
    }

    if (ehdr->e_type != ET_EXEC)      return ELF_UNSUPPORTED_TYPE;
    if (ehdr->e_machine != EM_X86_64) return ELF_UNSUPPORTED_MACHINE;

    if (ehdr->e_phoff == 0 || ehdr->e_phoff >= file_size) {
        return ELF_INVALID_PROGRAM_HEADER_OFFSET;
    }
    if (ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        return ELF_INVALID_PROGRAM_HEADER_SIZE;
    }

    uint64_t ph_table_bytes = (uint64_t)ehdr->e_phnum * sizeof(Elf64_Phdr);
    uint64_t ph_table_end = 0;
    if (ehdr->e_phnum == 0 ||
        add_overflow_u64(ehdr->e_phoff, ph_table_bytes, &ph_table_end) ||
        ph_table_end > file_size) {
        return ELF_INVALID_PROGRAM_HEADER_COUNT;
    }

    if (ehdr->e_entry == 0 || ehdr->e_entry >= USER_ADDR_MAX) {
        return ELF_UNSUPPORTED_TYPE;
    }

    return ELF_OK;
}

status program_load(const uint8_t* file_buf,
                    size_t file_size,
                    int argc,
                    char** argv,
                    elf_process_t* out_proc,
                    uint64_t* out_argv_user) {
    if (!out_proc) return ELF_INVALID_MAGIC;
    zero_mem(out_proc, sizeof(elf_process_t));

    status st = validate(file_buf, file_size, &out_proc->osabi);
    if (st != ELF_OK) {
        return st;
    }

    if (g_saved_page_count > 0) {
        restore_backed_up_pages();
    }

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)file_buf;
    const Elf64_Phdr* phdrs = (const Elf64_Phdr*)(file_buf + ehdr->e_phoff);

    out_proc->entry = ehdr->e_entry;
    out_proc->min_vaddr = ~0ULL;
    out_proc->max_vaddr = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* phdr = &phdrs[i];

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) {
            continue;
        }

        uint64_t file_end = 0;
        uint64_t mem_end = 0;
        if (phdr->p_memsz < phdr->p_filesz ||
            add_overflow_u64(phdr->p_offset, phdr->p_filesz, &file_end) ||
            file_end > file_size ||
            add_overflow_u64(phdr->p_vaddr, phdr->p_memsz, &mem_end) ||
            mem_end >= USER_ADDR_MAX) {
            program_unload(out_proc);
            return ELF_INVALID_PROGRAM_HEADER_OFFSET;
        }

        uint64_t seg_start = ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE);
        uint64_t seg_end   = ALIGN_UP(mem_end, PAGE_SIZE);

        if (seg_start < out_proc->min_vaddr) out_proc->min_vaddr = seg_start;
        if (seg_end   > out_proc->max_vaddr) out_proc->max_vaddr = seg_end;

        sprintf("[ELF] seg[%u]: vaddr=0x%llx memsz=0x%llx mapped=[0x%llx, 0x%llx) flags=0x%x\n",
                i,
                (unsigned long long)phdr->p_vaddr,
                (unsigned long long)phdr->p_memsz,
                (unsigned long long)seg_start,
                (unsigned long long)seg_end,
                phdr->p_flags);

        backup_range(seg_start, seg_end);

        for (uint64_t vaddr = seg_start; vaddr < seg_end; vaddr += PAGE_SIZE) {
            uint64_t phys = alloc_page();
            if (!phys) {
                sprintf("[ELF] ERROR: alloc_page failed for segment at 0x%llx\n",
                        (unsigned long long)vaddr);
                program_unload(out_proc);
                return ELF_INVALID_PROGRAM_HEADER_OFFSET;
            }

            if (!map_page(vaddr, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)) {
                sprintf("[ELF] ERROR: map_page failed for segment at 0x%llx\n",
                        (unsigned long long)vaddr);
                free_page(phys);
                program_unload(out_proc);
                return ELF_INVALID_PROGRAM_HEADER_OFFSET;
            }
            flush_tlb_page(vaddr);
        }

        zero_mem((void*)seg_start, (size_t)(seg_end - seg_start));

        if (phdr->p_filesz > 0) {
            copy_mem((void*)phdr->p_vaddr, file_buf + phdr->p_offset, (size_t)phdr->p_filesz);
        }

        uint64_t final_flags = PAGE_PRESENT | PAGE_USER;
        if (phdr->p_flags & PF_W)  final_flags |= PAGE_WRITABLE;
        if (!(phdr->p_flags & PF_X)) final_flags |= PTE_NX;

        for (uint64_t vaddr = seg_start; vaddr < seg_end; vaddr += PAGE_SIZE) {
            uintptr_t phys = unmap_page(vaddr);
            flush_tlb_page(vaddr);
            if (!phys || !map_page(vaddr, phys, final_flags)) {
                sprintf("[ELF] ERROR: remap failed for segment at 0x%llx\n",
                        (unsigned long long)vaddr);
                if (phys) free_page(phys);
                program_unload(out_proc);
                return ELF_INVALID_PROGRAM_HEADER_OFFSET;
            }
            flush_tlb_page(vaddr);
        }
    }

    uint64_t stack_bottom = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
    backup_range(stack_bottom, USER_STACK_TOP);

    for (uint64_t vaddr = stack_bottom; vaddr < USER_STACK_TOP; vaddr += PAGE_SIZE) {
        uint64_t phys = alloc_page();
        if (!phys) {
            sprintf("[ELF] ERROR: alloc_page failed for stack at 0x%llx\n",
                    (unsigned long long)vaddr);
            program_unload(out_proc);
            return ELF_INVALID_PROGRAM_HEADER_OFFSET;
        }

        if (!map_page(vaddr, phys, PAGE_USER_FLAGS | PTE_NX)) {
            sprintf("[ELF] ERROR: map_page failed for stack at 0x%llx\n",
                    (unsigned long long)vaddr);
            free_page(phys);
            program_unload(out_proc);
            return ELF_INVALID_PROGRAM_HEADER_OFFSET;
        }
        flush_tlb_page(vaddr);
    }

    zero_mem((void*)stack_bottom, USER_STACK_PAGES * PAGE_SIZE);

    uint64_t adjusted_sp = USER_STACK_TOP;
    uint64_t user_argv_ptr = 0;

    if (setup_user_stack_args(stack_bottom, USER_STACK_TOP, argc, argv,
                              &adjusted_sp, &user_argv_ptr) != 0) {
        sprintf("[ELF] ERROR: failed to place argc/argv on user stack\n");
        program_unload(out_proc);
        return ELF_INVALID_PROGRAM_HEADER_OFFSET;
    }

    out_proc->stack_top = adjusted_sp;
    if (out_argv_user) {
        *out_argv_user = user_argv_ptr;
    }

    sprintf("[ELF] Stack mapped: [0x%llx, 0x%llx) (%d pages), sp=0x%llx, argc=%d, argv=0x%llx\n",
            (unsigned long long)stack_bottom,
            (unsigned long long)USER_STACK_TOP,
            USER_STACK_PAGES,
            (unsigned long long)adjusted_sp,
            argc,
            (unsigned long long)user_argv_ptr);

    return ELF_OK;
}

static void free_scratch_buffer(uintptr_t vbase, size_t num_pages) {
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t va = vbase + (i * PAGE_SIZE);
        uintptr_t phys = unmap_page(va);
        flush_tlb_page(va);
        if (phys) {
            free_page(phys);
        }
    }
    g_current_scratch_pages = 0;
}

static status load_file_to_scratch(char* filename, size_t file_size, uint8_t** out_buf, size_t* out_pages) {
    if (!filename || file_size == 0) {
        sprintf("[ELF] Invalid filename or zero file size\n");
        return ELF_INVALID_PROGRAM_HEADER_OFFSET;
    }

    if (!fs_exists(filename)) {
        sprintf("[ELF] File not found: %s\n", filename);
        return ELF_INVALID_PROGRAM_HEADER_OFFSET;
    }

    size_t num_pages = ALIGN_UP(file_size, PAGE_SIZE) / PAGE_SIZE;
    g_current_scratch_pages = num_pages;

    for (size_t i = 0; i < num_pages; i++) {
        uint64_t phys = alloc_page();
        if (!phys) {
            sprintf("[ELF] Out of physical pages for ELF buffer\n");
            free_scratch_buffer(ELF_SCRATCH_VBASE, i);
            return ELF_INVALID_PROGRAM_HEADER_OFFSET;
        }

        uint64_t va = ELF_SCRATCH_VBASE + (i * PAGE_SIZE);
        if (!map_page(va, phys, PAGE_PRESENT | PAGE_WRITABLE)) {
            free_page(phys);
            sprintf("[ELF] Failed to map scratch page\n");
            free_scratch_buffer(ELF_SCRATCH_VBASE, i);
            return ELF_INVALID_PROGRAM_HEADER_OFFSET;
        }
        flush_tlb_page(va);
    }

    uint8_t* file_buf = (uint8_t*)ELF_SCRATCH_VBASE;
    if (fs_read_raw(filename, file_buf, file_size) < 0) {
        sprintf("[ELF] Failed to read file: %s\n", filename);
        free_scratch_buffer(ELF_SCRATCH_VBASE, num_pages);
        return ELF_INVALID_PROGRAM_HEADER_OFFSET;
    }

    *out_buf = file_buf;
    *out_pages = num_pages;
    return ELF_OK;
}

status spawn(char* filename,
             size_t file_size,
             int argc,
             char** argv,
             elf_process_t* out_proc,
             uint64_t* out_argv_user) {
    if (!out_proc) {
        return ELF_INVALID_MAGIC;
    }

    uint8_t* file_buf = NULL;
    size_t num_pages = 0;
    status read_st = load_file_to_scratch(filename, file_size, &file_buf, &num_pages);
    if (read_st != ELF_OK) {
        return read_st;
    }

    status st = program_load(file_buf, file_size, argc, argv, out_proc, out_argv_user);
    free_scratch_buffer(ELF_SCRATCH_VBASE, num_pages);

    if (st != ELF_OK) {
        sprintf("[ELF] Spawn failed for %s: %s\n", filename, elf_error_messages[st]);
        return st;
    }

    sprintf("[ELF] Spawned %s (entry=0x%llx, stack=0x%llx, argc=%d, osabi=0x%x)\n",
            filename,
            (unsigned long long)out_proc->entry,
            (unsigned long long)out_proc->stack_top,
            argc,
            out_proc->osabi);

    return ELF_OK;
}

void run(char* filename, size_t file_size, int argc, char** argv) {
    elf_process_t proc;
    uint64_t user_argv_ptr = 0;

    char* default_argv[1];
    if (argc == 0 && argv == NULL && filename != NULL) {
        default_argv[0] = filename;
        argc = 1;
        argv = default_argv;
    }

    status st = spawn(filename, file_size, argc, argv, &proc, &user_argv_ptr);
    if (st != ELF_OK) {
        return;
    }

    sprintf("[ELF] Running %s (entry=0x%llx, stack=0x%llx, argc=%d, argv=0x%llx, osabi=0x%x)\n",
            filename,
            (unsigned long long)proc.entry,
            (unsigned long long)proc.stack_top,
            argc,
            (unsigned long long)user_argv_ptr,
            proc.osabi);

    to_userland_ring3(proc.entry, proc.stack_top & ~0xFULL, argc, user_argv_ptr);

    program_unload(&proc);
    sprintf("[ELF] Process %s exited and unloaded\n", filename);
}