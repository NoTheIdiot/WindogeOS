#include <system.h>
#include <stdint.h>
#include <string.h>
#include <boot/limine.h>
#include <dogeio.h>
#include <bool.h>
#include <stddef.h>
#include <basicutil.h>
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

    log("WindogeOS has successfully booted. Start celebrating broski.");
    menubar_draw();

    if (!fs_exists(".settings_color")) {
        fs_create(".settings_color");
    }

    if (!fs_exists(".settings")) {
        fs_create(".settings");
    }

    char saved_color[16];
    fs_read(".settings_color", saved_color, 16);

    if (str_strcmp(saved_color, "black") == 0) {
        dogeio_text_color = 0xFF000000;
    } else if (str_strcmp(saved_color, "white") == 0) {
        dogeio_text_color = 0xFFFFFFFF;
    } else if (str_strcmp(saved_color, "grey") == 0) {
        dogeio_text_color = 0xFF808080;
    } else if (str_strcmp(saved_color, "dark_grey") == 0) {
        dogeio_text_color = 0xFF404040;
    } else if (str_strcmp(saved_color, "red") == 0) {
        dogeio_text_color = 0xFFFF0000;
    } else if (str_strcmp(saved_color, "green") == 0) {
        dogeio_text_color = 0xFF00FF00;
    } else if (str_strcmp(saved_color, "blue") == 0) {
        dogeio_text_color = 0xFF0000FF;
    } else if (str_strcmp(saved_color, "yellow") == 0) {
        dogeio_text_color = 0xFFFFFF00;
    } else if (str_strcmp(saved_color, "cyan") == 0) {
        dogeio_text_color = 0xFF00FFFF;
    } else if (str_strcmp(saved_color, "magenta") == 0) {
        dogeio_text_color = 0xFFFF00FF;
    } else if (str_strcmp(saved_color, "navy") == 0) {
        dogeio_text_color = 0xFF000080;
    } else if (str_strcmp(saved_color, "maroon") == 0) {
        dogeio_text_color = 0xFF800000;
    } else if (str_strcmp(saved_color, "teal") == 0) {
        dogeio_text_color = 0xFF008080;
    } else if (str_strcmp(saved_color, "olive") == 0) {
        dogeio_text_color = 0xFF808000;
    } else if (str_strcmp(saved_color, "doge_gold") == 0) {
        dogeio_text_color = 0xFFE1B857;
    } else if (str_strcmp(saved_color, "doge_tan") == 0) {
        dogeio_text_color = 0xFFF4DFB1;
    } else {
        dogeio_text_color = 0xFFFFFFFF;
    }

    const char* starting[6] = {
		"================================================================================================================================================================",
		"=                                                                                                                                                              =",
		"=                                                                 Welcome to WindogeOS v0.0.3!                                                                 =",
		"=                                                          Type 'help' for more help in the dogeshell                                                          =",
		"=                                                                                                                                                              =",
		"================================================================================================================================================================"
	};

    for (int i = 0; i < 6; i++) {
        dogeio_text_print(starting[i]);
    }

    char shell[32];
    fs_read(".settings", shell, 32);

    if (str_strcmp(shell, "dogeshell") == 0) {
        log("Starting Dogeshell");
        system_dogeshell();
    } else if (str_strcmp(shell, "sbash") == 0) {
        log("Starting bash");
        system_bash();
    } else {
        log("Starting Dogeshell");
        system_dogeshell();
    }

    halt();
}
