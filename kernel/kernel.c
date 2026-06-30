#include <stdint.h>
#include <stddef.h>
#include <bool.h>
#include <limine.h>
#include <dogeio.h>
#include <system.h>
#include <string.h>
#include <time.h> 

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

extern void system_dogeshell();

void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }
    
    dogeio_text_clear();
    time_rtc_init();
    if (module_request.response == NULL || module_request.response->module_count == 0) {
        dogeio_text_println("now wow: limine boot modules not found.");
    }

    int status = system_file_init(module_request.response);
    if (status != 0) {
        dogeio_text_println("not wow: filesystem context init failed.");
        dogeio_text_println("fat32 will not be avaliable.");
    }

    dogeio_text_println("Welcome to WindogeOS Bliss! V0.0.2 L2");
    system_dogeshell();
    hcf();
}
