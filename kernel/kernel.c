#include <stdint.h>
#include <stddef.h>
#include <bool.h>
#include <limine.h>
#include <dogeio.h>
#include <system.h>
#include <string.h>
#include <time.h> 

void init_kernel_fpu(void) {
#if defined(__x86_64__)
    unsigned long cr0, cr4;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);
    cr0 |=  (1ULL << 1);
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10);
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));

#elif defined(__aarch64__)
    unsigned long cpacr;

    __asm__ volatile("mrs %0, cpacr_el1" : "=r"(cpacr));
    
    cpacr |= (3ULL << 20);
    
    __asm__ volatile(
        "msr cpacr_el1, %0\n\t"
        "isb"
        :: "r"(cpacr) : "memory"
    );
#endif
}


__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_module_request module_request = {
    .id = { 0x67cf3d9d78a91a1e, 0x3bd7d9e63d902f06 }, 
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = { 0x67cf3d9d78a91a1e, 0x9fb54d6d67cf3d9d },
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

// menu bar function
void draw_menu_bar() {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = (uint32_t *)fb->address;
    size_t row_width = fb->pitch / 4;

    uint32_t bar_color = 0xffffff;
    uint32_t text_color = 0x000000;

    for (int row = 0; row < 16; row++) {
        for (size_t col = 0; col < fb->width; col++) {
            fb_ptr[row * row_width + col] = bar_color;
        }
    }

    dogeio_text_print_at("WindogeOS Bliss V0.0.2", 8, 0, text_color);

    char *clock_text = time_get();
    uint32_t clock_x = fb->width > 72 ? fb->width - 72 : 8;
    dogeio_text_print_at(clock_text, clock_x, 0, text_color);
}


void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }
    
    dogeio_text_clear();
    time_rtc_init();
    draw_menu_bar();
    init_kernel_fpu();
    if (module_request.response == NULL || module_request.response->module_count == 0) {
        dogeio_duolog("Not Wow: No Limine boot modules found.");
    }

    int status = system_file_init(module_request.response);
    if (status != 0) {
        dogeio_duolog("Not Wow: Failed to Intialize File System.");
        dogeio_duolog("File System will not be avaliable.");
    }
    dogeio_text_println("Waiting 3 seconds");
    time_wait_sec(3);

    dogeio_text_println("Welcome to WindogeOS Bliss! V0.0.2 L2");
    dogeio_log("WindogeOS Boot Success, start celebrating.");
    system_dogeshell();
    hcf();
}
