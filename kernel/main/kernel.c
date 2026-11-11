#include <limine.h>
#include <bool.h>
#include <system.h>
#include <string.h>
#include <dogeio.h>

__attribute__((used, section(".limine_requests_start")))
volatile uint64_t limine_request_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
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
volatile uint64_t limine_request_end_marker[] = LIMINE_REQUESTS_END_MARKER;

extern void backup_shell(void);

void windoge_os(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }
    cursor_show();
    clear();
    println("[\033[32mDoge\033[0m]    WindogeOS started booting");
    println("[\033[32mDogeing\033[0m] Init GDT and TSS...");
    init_gdt_tss();
    println("[\033[32mWow\033[0m]     Init GDT and TSS sucessful");
    println("[\033[32mDogeing\033[0m] Init IDT...");
    init_idt();
    println("[\033[32mWow\033[0m]     Init IDT sucessful");
    println("[\033[32mDogeing\033[0m] Init serial...");
    serial_init();
    println("[\033[32mWow\033[0m]     Init serial sucess");
    sprintln("Testing serial");
    println("[\033[32mDogeing\033[0m] Init APIC...");
    init_apic();
    println("[\033[32mWow\033[0m]     Init APIC sucessful");
    init_vmm();

    println("[\033[32mDogeing\033[0m] Init PCIE...");
    init_pcie();
    println("[\033[32mWow\033[0m]     Init PCIE done (still didnt crash nice)");

    println("[\033[32mDogeing\033[0m] Probing AHCI SATA...");
    ata_probe_devices();
    println("[\033[32mWow\033[0m]     AHCI SATA probed");

    println("[\033[32mDogeing\033[0m] Mounting exFAT filesystem...");
    if (fs_mount()) {
        println("[\033[91mNot Wow\033[0m] didn't mount, god damn it.");
        halt();
    }
    println("[\033[32mWow\033[0m]     Mounted filesystem");

    println("[\033[32mDogeing\033[0m] Trying some USB 1.1 bullshit...");
    uhci_init_driver();
    uhci_poll_ports();
    init_usb_keyboard(1);
    println("[\033[32mWow\033[0m]     Holy shit it works (sucess)");

    println("[\033[32mDogeing\033[0m] Init RTC time..");
    init_rtc_time();
    println("[\033[32mWow\033[0m]     Init RTC sucess");
    println("[\033[32mDogeing\033[0m] Updating RTC time..");
    update_time();
    println("[\033[32mWow\033[0m]     Updated RTC time");

    print("[\033[32mDoge\033[0m]    Current Date: ");
    println(get_date());
    print("[\033[32mDoge\033[0m]    Current Time: ");
    println(get_time());

    println("[\033[32mDogeing\033[0m] Init system calls...");
    init_syscalls();
    println("[\033[32mWow\033[0m]     Init system calls done");

        println("\n[\033[96mDoge\033[0m]    Start to test drivers");
    println("[\033[96mDoge\033[0m]    Testing exFAT");

    char test_file[] = "TEST";

    if (fs_create(test_file) == 0) {
        println("[\033[32mWow\033[0m]     File creation works");
    } else {
        println("[\033[91mNot Wow\033[0m] File creation failed");
    }

    println("[\033[96mDoge\033[0m]    Testing WRITE");
    char msg1[] = "WINDOGEOS BEST";
    if (fs_write(test_file, msg1, sizeof(msg1)) == 0) {
        println("[\033[32mWow\033[0m]     Write works");
    } else {
        println("[\033[91mNot Wow\033[0m] Write failed");
    }

    println("[\033[96mDoge\033[0m]    Testing READ");
    char buffer[64];
    fs_read(test_file, buffer, 64);
    println(buffer);

    println("[\033[96mDoge\033[0m]    Testing OVERWRITE");
    char msg2[] = "I HATE BAD DOCS";
    fs_write(test_file, msg2, 16);

    memset(buffer, 0, sizeof(buffer));
    fs_read(test_file, buffer, 64);
    println(buffer);

    clear();
    println("\033[96mWindogeOS Booted!\033[0m");
    println("\033[96mThere is literally nothing else mate... other than a \ntesting shell (like kdb)\033[0m");
    backup_shell();
    halt();
}