#include <system.h>
#include <stdint.h>
#include <string.h>
#include <boot/limine.h>
#include <dogeio.h>
#include <bool.h>
#include <stddef.h>
#include <basicutil.h>
#include <system.h>
#include <boot/kernel.h>

__attribute__((used, section(".limine_requests_start")))
volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

void kernel_main(void) {
    serial_init();
    log("[Wow] Serial Initialize Sucess, very wow.");

    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        log("[Not Wow] Limine's base literally doesn't exist");
        log("          LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false");
        halt();
    }
    log("[Wow] Limine's base revision exists, much wow.");

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        log("[Not Wow] Limine's framebuffer literally doesn't exist");
        log("		   framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1");
        halt();
    }
    log("[Wow] Limine's Framebuffer Initialize Sucess, very very wow.");

	if (memmap_request.response == NULL) {
		log("[Not Wow] Holy shit RAM does not exist.... How?");
		log("          memmap_request.response == NULL");
		halt();
	}
	log("[Wow] Memory Map has been found.");

	log("Formating ramisk, such patience");
	system_fs_start_init();
	log("[Wow] Format Sucess");

    log("WindogeOS has successfully booted. Start celebrating broski.");
	menubar_draw();
    dogeio_text_print("============================================================================\n");
    dogeio_text_print("=                       Welcome to WindogeOS! v0.0.3                       =\n");
    dogeio_text_print("=                  Type 'help' for help in the dogeshell.                  =\n");
    dogeio_text_print("============================================================================\n");
    log("Starting Dogeshell");
    system_dogeshell();
    halt();
}
