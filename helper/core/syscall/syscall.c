#include <stdint.h>
#include <core.h>
#include <basicutil.h>
#include <system.h>
#include <dogeio.h>
#include <boot/kernel.h>
#include <boot/limine.h>

#define IA32_EFER    0xC0000080
#define IA32_STAR    0xC0000081
#define IA32_LSTAR   0xC0000082
#define IA32_FMASK   0xC0000084
#define IA32_KERNEL_GS_BASE 0xC0000102

static uint8_t kernel_stack[16384] __attribute__((aligned(16)));
typedef struct {
    uint64_t kernel_stack_top;
    uint64_t user_rsp_scratch;
} __attribute__ ((aligned(16))) gs_data;

extern void syscall_entry(void);
extern void core_to_user(uint64_t user_pml4_phys, void *user_entry, void *user_stack_top);

extern volatile struct limine_framebuffer_request framebuffer_request;
static gs_data per_core_gs __attribute__((aligned(16)));

[[maybe_unused]] static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" :: "a"(low), "d"(high), "c"(msr) : "memory");
}

void init_syscall(void) {
    uint64_t top = (uint64_t)(kernel_stack + sizeof(kernel_stack));
    top &= ~0xFULL;
    per_core_gs.kernel_stack_top = top;
    per_core_gs.user_rsp_scratch = 0;

    wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)&per_core_gs);

    uint64_t efer = rdmsr(IA32_EFER);
    efer |= (1 << 0);
    wrmsr(IA32_EFER, efer);

    uint64_t star = ((uint64_t)0x10 << 32) | ((uint64_t)0x08 << 48);
    wrmsr(IA32_STAR, star);

    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(IA32_FMASK, 0x200);
}

uint64_t c_syscall_handler(
    [[maybe_unused]] uint64_t sys_id, 
    [[maybe_unused]] uint64_t arg1, 
    [[maybe_unused]] uint64_t arg2, 
    [[maybe_unused]] uint64_t arg3, 
    [[maybe_unused]] uint64_t arg4
) {
    switch (sys_id) {
        case DOGEIO_TEXT_PUTCHAR:
            dogeio_text_putchar((char)arg1, (uint32_t)arg2, (uint32_t)arg3);
            return 0;
        case DOGEIO_TEXT_CLEAR:
            dogeio_text_clear();
            return 0;
        case DOGEIO_TEXT_PRINTCHAR:
            dogeio_text_printchar((char)arg1);
            return 0;
        case DOGEIO_TEXT_PRINT:
            dogeio_text_print((const char *)arg1);
            return 0;
        case DOGEIO_TEXT_PRINTLN:
            dogeio_text_println((const char *)arg1);
            return 0;
        case DOGEIO_TEXT_PRINT_AT:
            dogeio_text_print_at((const char *)arg1, (uint32_t)arg2, (uint32_t)arg3, (uint32_t)arg4);
            return 0;
        case DOGEIO_TEXT_INPUT:
            dogeio_text_input((const char *)arg1, (char *)arg2, (size_t)arg3);
            return 0;
        case DOGEIO_TEXT_COLOR_CHANGE:
            dogeio_text_color_change((uint32_t)arg1);
            return 0;
        case DOGEIO_TEXT_BACKGROUND_CHANGE:
            dogeio_text_background_change((uint32_t)arg1);
            return 0;
        case DOGEIO_TEXT_CLEAR_RAW:
            dogeio_text_clear_raw();
            return 0;

        case DOGEIO_FS_FORMAT:
            return (uint64_t)fs_format();
        case DOGEIO_FS_CREATE:
            return (uint64_t)fs_create((char *)arg1);
        case DOGEIO_FS_MKDIR:
            return (uint64_t)fs_mkdir((char *)arg1);
        case DOGEIO_FS_EXISTS:
            return (uint64_t)fs_exists((char *)arg1);
        case DOGEIO_FS_DELETE:
            return (uint64_t)fs_delete((char *)arg1);
        case DOGEIO_FS_DELETE_LAST_LINE:
            return (uint64_t)fs_delete_last_line((char *)arg1);
        case DOGEIO_FS_READ:
            return (uint64_t)fs_read((char *)arg1, (char *)arg2, (uint32_t)arg3);
        case DOGEIO_FS_WRITE:
            return (uint64_t)fs_write((char *)arg1, (char *)arg2);
        case DOGEIO_FS_LIST_DIR:
            return (uint64_t)fs_list_dir((int)arg1);
        case DOGEIO_FS_RENAME:
            return (uint64_t)fs_rename((char *)arg1, (char *)arg2);
        case DOGEIO_FS_COPY:
            fs_copy((char *)arg1, (char *)arg2);
            return 0;
        case DOGEIO_FS_CHDIR:
            return (uint64_t)fs_chdir((char *)arg1);
        case DOGEIO_FS_DIRNAME:
            return (uint64_t)(uintptr_t)fs_dirname();
        case DOGEIO_FS_MOUNT:
            return (uint64_t)fs_mount();

        case DOGEIO_EXEC_FLAT_BINARY:
            return (uint64_t)exec_flat_binary((const char *)arg1, (int)arg2, (char **)arg3);

        case KERNEL_MENUBAR_DRAW:
            menubar_draw();
            return 0;

        case KERNEL_EXFAT_WIPE_AND_FORMAT:
            return (uint64_t)exfat_wipe_and_format();

        case KERNEL_EXFAT_CREATE_NODE:
            return (uint64_t)exfat_create_node((const char*)arg1, (bool)arg2);

        case KERNEL_EXFAT_WRITE_FILE:
            return (uint64_t)exfat_write_file((const char*)arg1, (const uint8_t*)arg2, arg3);

        case KERNEL_EXFAT_APPEND_FILE:
            return (uint64_t)exfat_append_file((const char*)arg1, (const uint8_t*)arg2, arg3);

        case KERNEL_EXFAT_READ_FILE:
            return (uint64_t)exfat_read_file((const char*)arg1, (uint8_t*)arg2, arg3);

        case KERNEL_EXFAT_DELETE_NODE:
            return (uint64_t)exfat_delete_node((const char*)arg1);

        case KERNEL_EXFAT_TRUNCATE_LAST_LINE:
            return (uint64_t)exfat_truncate_last_line((const char*)arg1);

        case KERNEL_EXFAT_PRINT_DIRECTORY:
            return (uint64_t)exfat_print_directory((int)arg1);

        case KERNEL_EXFAT_CHANGE_DIRECTORY:
            return (uint64_t)exfat_change_directory((const char*)arg1);

        case KERNEL_EXFAT_GET_WORKING_DIR:
            return (uint64_t)exfat_get_working_dir();

        case KERNEL_EXFAT_MOUNT:
            return (uint64_t)exfat_mount();

        case KERNEL_PUT_PIXEL: {
            uint64_t x = arg1;
            uint64_t y = arg2;
            uint32_t color = (uint32_t)arg3;

            struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
            if (x < fb->width && y < fb->height) {
                volatile uint32_t *pixel_address = (volatile uint32_t*)((uint8_t*)fb->address + (y * fb->pitch) + (x * 4));
                *pixel_address = color;
            }
            return 0;
        }   
        
        default:
            return (uint64_t)-1;
    }

    return (uint64_t)-5;
}