#include <system.h>
#include <stdint.h>
#include <string.h>
#include <boot/limine.h>
#include <dogeio.h>
#include <bool.h>
#include <stddef.h>
#include <basicutil.h>
#include <core.h>
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
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
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
        panic("Do not use fish as your bootloader", __FILE__, __LINE__);
    }
    log("[Wow] Limine's base revision exists, much wow.");

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        log("[Not Wow] Limine's framebuffer literally doesn't exist");
        log("          framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1");
        panic("Where are your framebuffers?", __FILE__, __LINE__);
    }
    log("[Wow] Limine's Framebuffer Initialize Sucess, very very wow.");

    if (memmap_request.response == NULL) {
        log("[Not Wow] Holy shit RAM does not exist.... How?");
        log("          memmap_request.response == NULL");
        panic("Memmap for some reason isn't here", __FILE__, __LINE__);
    }
    log("[Wow] Memory Map has been found.");

    log("Mounting Drive");
    if (fs_mount()) {
        log("Drive mounted");
    } else {
        log("Something went wrong, guess im formating");
        fs_format();
    }

    log("Init gdt idt tss and syscall");
    init_gdt();
    init_idt();
    init_syscalls();
    log("Success");

    log("WindogeOS has successfully booted. Start celebrating broski.");
    menubar_draw();

    if (!fs_exists(".windoge")) {
        fs_format();
        fs_mount();

        dogeio_text_println("Welcome to WindogeOS setup. (Version WindogeOS v0.01)");
        dogeio_text_println("Such doge very OS. Much cheems many veichular manslaughter.");
        dogeio_text_println("Press enter to continue.");

        char nothing[1];
        dogeio_text_input("", nothing, 1);

        fs_mkdir("system");
        fs_mkdir("users");
        fs_chdir("system");
        fs_mkdir("pass");
        fs_chdir("/");
        fs_create(".windoge");

        char username_new[64];
        char password_new[64];

        char root_new[64];
        fs_chdir("/system/pass");

        dogeio_text_println("Enter root password. You'll need this for admin level privages.");
        dogeio_text_input("root password (new)> ", root_new, 64);
        fs_create("root.hash");
        fs_write("root.hash", root_new);
        dogeio_text_println("");
        fs_chdir("/");

        dogeio_text_println("Create a new user. Not sure why anyone would daily drive since it's useless...");
        dogeio_text_input("new username> ", username_new, 64);
        dogeio_text_input("new password> ", password_new, 64);
        system_create_user(username_new, password_new);

        dogeio_text_println("\nSetup complete. Press enter to start system.");
        dogeio_text_input("", nothing, 1);
    }

    dogeio_text_clear();
    dogeio_text_println("Locked screen.");
    
    char username[64];
    while (true) {
        char password[64];

        dogeio_text_input("username> ", username, 64);
        dogeio_text_input("password> ", password, 64);

        if (system_verify_user(username, password)) {
            break;
        }
        dogeio_text_println("Wrong password or user doesn't exist :(");
    }
    
    char userpath[128];
    str_strcpy(userpath, "/users/");
    str_strcat(userpath, username);
    fs_chdir(userpath);

    dogeio_text_clear();
    
    const char* starting[6] = {
        "================================================================================================================================================================",
        "=                                                                                                                                                              =",
        "=                                                     Welcome to WindogeOS v0.0.5-Build3!                                                                      =",
        "=                                                        Type 'help' for more help in the dogeshell                                                            =",
        "=                                                                                                                                                              =",
        "================================================================================================================================================================"
    };

    for (int i = 0; i < 6; i++) {
        dogeio_text_print(starting[i]);
    }

    log("Starting Dogeshell");
    system_dogeshell();

    halt();
}