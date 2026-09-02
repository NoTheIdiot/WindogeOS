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
        fs_mount();
        fs_set_auth_override(1);

        dogeio_text_println("Welcome to WindogeOS setup");
        dogeio_text_println("Version: WindogeOS v0.01");

        char proceed[2];
        dogeio_text_input("Press Enter to continue> ", proceed, 2);

        if (!fs_exists("system")) {
            fs_mkdir("system");
        }
        if (!fs_exists("users")) {
            fs_mkdir("users");
        }

        fs_chdir("/system");
        if (!fs_exists("pass")) {
            fs_mkdir("pass");
        }
        if (!fs_exists("root")) {
            fs_mkdir("root");
        }
        fs_chdir("/");
        fs_create(".windoge");

        fs_chdir("/users");
        if (!fs_exists("admin")) {
            fs_mkdir("admin");
        }
        fs_chdir("/");

        char username_new[64];
        char password_new[64];
        char root_new[64];

        while (true) {
            dogeio_text_println("\nCreate the root password.");
            dogeio_text_input("root password> ", root_new, 64);
            if (root_new[0] == '\0') {
                dogeio_text_println("Password cannot be empty.");
                continue;
            }
            break;
        }

        fs_chdir("/system/pass");
        fs_create("root.hash");
        fs_write("root.hash", root_new);
        fs_create("admin.hash");
        fs_write("admin.hash", root_new);
        fs_chdir("/");

        while (true) {
            dogeio_text_println("username must be lowercase and non-empty.");
            dogeio_text_input("username> ", username_new, 64);

            for (int i = 0; username_new[i] != '\0'; i++) {
                if (username_new[i] >= 'A' && username_new[i] <= 'Z') {
                    username_new[i] += 32;
                }
                if (username_new[i] == ' ' || username_new[i] == '\t' || username_new[i] == '\n' || username_new[i] == '\r') {
                    username_new[i] = '\0';
                    break;
                }
            }

            if (username_new[0] == '\0') {
                dogeio_text_println("username cannot be empty.");
                continue;
            }

            dogeio_text_input("password> ", password_new, 64);
            if (password_new[0] == '\0') {
                dogeio_text_println("password cannot be empty.");
                continue;
            }
            break;
        }

        system_create_user(username_new, password_new);
        fs_chdir("/users");
        if (!fs_exists("admin")) {
            fs_mkdir("admin");
        }
        fs_chdir("/");

        dogeio_text_clear();
        char nothing[1];
        dogeio_text_input("Press enter to reboot.", nothing, 1);
        fs_set_auth_override(0);
        core_reboot();
    }

    fs_set_auth_override(1);
    dogeio_text_println("Welcome to WindogeOS v0.01");

    char username[64];
    while (true) {
        char password[64];

        dogeio_text_input("username> ", username, 64);
        dogeio_text_input("password> ", password, 64);

        if (system_verify_user(username, password)) {
            str_strcpy(current_user, username);
            fs_set_auth_override(0);
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

    core_shutdown();
}