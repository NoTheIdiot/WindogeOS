#include <system.h>
#include <stdint.h>
#include <string.h>
#include <boot/limine.h>
#include <dogeio.h>
#include <bool.h>
#include <stddef.h>
#include <math.h>
#include <start.h>
#include <basicutil.h>
#include <boot/kernel.h>
#include <sysinfo.h>

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

// random things that are probably too messy for the headers
extern void load_settings(void);
extern void start_setup(void);
// end

// random flag variables
char computer_name[64];
bool is_setup = false;
// end

static void install_root_apps(void) {
    static char root_files[48][256];

    if (!fs_exists("/apps") && fs_mkdir("/apps") != 0) {
        log("[setup] unable to create /apps");
        return;
    }

    int file_count = fs_list_files("/", root_files, 
                                   sizeof(root_files) / sizeof(root_files[0]));
    if (file_count < 0) {
        log("[setup] unable to list root files");
        return;
    }

    int copied_count = 0;
    for (int i = 0; i < file_count; i++) {
        const char *name = root_files[i];
        size_t name_length = str_strlen(name);
        if (name_length < 4) {
            continue;
        }

        const char *extension = name + name_length - 4;
        if (extension[0] != '.' ||
           (extension[1] != 'b' && extension[1] != 'B') ||
           (extension[2] != 'i' && extension[2] != 'I') ||
           (extension[3] != 'n' && extension[3] != 'N')) {
            continue;
        }

        char source[sizeof("/") + sizeof(root_files[0])];
        char destination[sizeof("/apps/") + sizeof(root_files[0])];
        str_strcpy(source, "/");
        str_strcat(source, name);
        str_strcpy(destination, "/apps/");
        str_strcat(destination, name);

        if (fs_copy(source, destination) != 0 ||
            !fs_exists(destination)) {
            serial_print("[setup] failed to copy app into /apps: ");
            serial_print(name);
            serial_print("\n");
            continue;
        }

        if (fs_delete(source) != 1) {
            serial_print("[setup] copied app but failed to delete root original: ");
            serial_print(name);
            serial_print("\n");
            continue;
        }

        copied_count++;
        serial_print("[setup] copied app into /apps and deleted root original: ");
        serial_print(name);
        serial_print("\n");
    }

    char count_string[16];
    str_itoa(copied_count, count_string);
    serial_print("[setup] apps copied to /apps and removed from root: ");
    serial_print(count_string);
    serial_print("\n");
}

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
        dogeio_println("[Not Wow] Memmap request does not respond. Not Wow.");
        cli();
        halt();
    }
    log("[Wow] Memory Map has been found.");

    log("Mounting Drive");
    if (fs_mount()) {
        log("Drive mounted");
    } else {
        log("Something went wrong, guess im formatting");
        fs_format();
        if (!fs_mount()) {
            log("[setup] unable to mount filesystem after formatting");
            halt();
        }
    }

    log("[setup] installing root applications");
    install_root_apps();

    log("[dogeing] initializing gdt + tss");
    init_gdt_tss();
    log("[dogeing] initializing idt");
    init_idt();
    log("[dogeing] initializing PIT");
    init_pit(1000);
    log("[dogeing] initializing syscalls");
    init_syscalls();
    log("[dogeing] initializing pcie");
    init_pcie();
    log("[dogeing] initializing terminal workspaces");
    sys_init_terminals();
    log("[dogeing] initializing floating point");
    fpu_init();
    log("[dogeing] initializing nvme (if any available)");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                
                uint32_t id_reg = pcie_read32((uint8_t)bus, device, function, 0x00);
                uint16_t vendor_id = (uint16_t)(id_reg & 0xFFFF);
                uint16_t device_id = (uint16_t)(id_reg >> 16);

                if (vendor_id == 0xFFFF) {
                    if (function == 0) break;
                    // else...
                    continue;
                }

                uint32_t class_reg = pcie_read32((uint8_t)bus, device, function, 0x08);
                uint8_t class_code = (uint8_t)((class_reg >> 24) & 0xFF);
                uint8_t subclass   = (uint8_t)((class_reg >> 16) & 0xFF);

                if (class_code == 0x01 && subclass == 0x08) {
                    nvme_pci_callback((uint8_t)bus, device, function, vendor_id, device_id);
                }
            }
        }
    }
    log("[wow] all basic drivers done");

    log("WindogeOS fully booted. very wow. (clear screen and draw status bar)");
    dogeio_clear();
    menubar_draw();

    // enable interrupts
    sti();

    if (!fs_exists(".windoge")) {
        install_root_apps();
        start_setup();
    }

    // load settings
    system_load_settings();
    fs_set_auth_override(1);

    char username[64];
    dogeio_color_change(0xE1B16C);
    for (int i = 0; i < 24; i++) {
        dogeio_print(" ");
        dogeio_println(doge_ascii_full[i]);
    }
    dogeio_color_change(COLOR_GRAY);
    dogeio_println(" Welcome back to WindogeOS!");
    while (true) {
        char password[64];

        // looks nicer...
        dogeio_input(" [username]> ", username, 64);
        dogeio_input(" [password]> ", password, 64);

        clean_input_string(username);
        clean_input_string(password);

        if (system_verify_user(username, password)) {
            str_strcpy(current_user, username);
            fs_set_auth_override(0);
            break;
        }
        dogeio_println("wrong username or password :(");
    }

    system_load_settings();

    char userpath[128];
    str_strcpy(userpath, "/users/");
    str_strcat(userpath, username);
    fs_chdir(userpath);

    dogeio_clear();
    
    char buffer[1024];
    fs_read("/apps/text/boot.txt", buffer, 1024);
    dogeio_print(buffer);

    log("Starting the default shell");
    start_shell();

    core_shutdown();
}
