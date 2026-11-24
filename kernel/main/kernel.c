#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <io.h>
#include <fonts.h>
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

typedef struct __attribute__((packed)) {
    uint64_t kernel_stack;
    uint64_t user_stack_scratch;
} cpu_gs_t;

static cpu_gs_t bootstrap_cpu_gs;

void init_cpu_gs(uint64_t kernel_stack_top) {
    bootstrap_cpu_gs.kernel_stack = kernel_stack_top;
    bootstrap_cpu_gs.user_stack_scratch = 0;

    uint64_t gs_base = (uint64_t)&bootstrap_cpu_gs;
    __asm__ volatile (
        "wrmsr"
        :
        : "c" (0xC0000102),
          "a" ((uint32_t)(gs_base & 0xFFFFFFFF)),
          "d" ((uint32_t)(gs_base >> 32))
    );
}

static void login_screen(void) {
    char username[MAX_USERNAME];
    char password[64];

    user_logout();

    while (1) {
        clear();
        printf("WindogeOS login\n");

        input("Username: ", username, sizeof(username));
        input_masked("Password: ", password, sizeof(password), true);

        if (user_login(username, password) >= 0) {
            printf("\n[\033[92m wow \033[0m] Logged in as %s\n\n", user_current_name());
            return;
        }

        printf("\n[\033[91m bad \033[0m] Invalid username or password. Press any key...\n");
        wait_for_next();
    }
}

static void create_account_setup(void) {
    char username[MAX_USERNAME];
    char password[64];
    char confirm[64];

    while (1) {
        clear();
        printf("WindogeOS Account Setup\n\n");

        input("Username: ", username, sizeof(username));
        if (username[0] == '\0') {
            printf("\n[\033[91m bad \033[0m] Username cannot be empty. Press any key...\n");
            wait_for_next();
            continue;
        }

        input_masked("Password: ", password, sizeof(password), true);
        input_masked("Confirm password: ", confirm, sizeof(confirm), true);

        if (strcmp(password, confirm) != 0) {
            printf("\n[\033[91m bad \033[0m] Passwords do not match. Press any key...\n");
            wait_for_next();
            continue;
        }

        int rc = user_add(username, password, 1);
        if (rc < 0) {
            printf("\n[\033[91m bad \033[0m] Could not create account (rc=%d). Press any key...\n", rc);
            wait_for_next();
            continue;
        }

        if (user_login(username, password) < 0) {
            printf("\n[\033[91m bad \033[0m] Account created, but login failed. Press any key...\n");
            wait_for_next();
            continue;
        }

        fs_create(".windoge");
        printf("\n[\033[92m wow \033[0m] Account '%s' ready.\n\n", user_current_name());
        return;
    }
}

// temporal start
static void temp(void) {
    printf("\"wow\" is the username and password for login.\n");
    printf("press any key to continue\n");
    wait_for_next();
}

void windoge(void) {
    clear();
    printf("Welcome to WindogeOS\n");
    init_serial();
    printf("[\033[96m ... \033[0m] Init GDT and TSS\n");
    init_gdt_tss();
    printf("[\033[92m wow \033[0m] GDT loaded\n");
    printf("[\033[96m ... \033[0m] Init IDT\n");
    init_idt();
    printf("[\033[92m wow \033[0m] IDT loaded\n");
    printf("[\033[96m ... \033[0m] Init FPU\n");
    init_fpu();
    printf("[\033[92m wow \033[0m] FPU done\n");
    printf("[\033[96m ... \033[0m] Init VMM and PMM\n");
    init_memory();
    printf("[\033[92m wow \033[0m] VMM and PMM done\n");
    printf("[\033[96m ... \033[0m] Init APIC\n");
    if (!init_apic() || !init_apic_timer(32, 1)) {
        printf("[\033[91m bad \033[0m] APIC init failed\n");
        halt();
    }
    printf("[\033[92m wow \033[0m] APIC done\n");
    printf("[\033[96m ... \033[0m] Init CPU GS\n");
    
    uint64_t current_rsp;
    __asm__ volatile ("movq %%rsp, %0" : "=r"(current_rsp));
    init_cpu_gs(current_rsp);
    
    printf("[\033[92m wow \033[0m] Init CPU GS sucessful\n");

    printf("[\033[96m ... \033[0m] Init PCIE...\n");
    init_pcie();
    printf("[\033[92m wow \033[0m] PCIE done\n");
    printf("[\033[96m ... \033[0m] Init AHCI (and SATA)...\n");
    ata_probe_devices();
    printf("[\033[92m wow \033[0m] AHCI and SATA done\n");
    printf("[\033[96m ... \033[0m] Init USB\n");
    init_keyboard();
    printf("[\033[92m wow \033[0m] USB done\n");
    printf("[\033[96m ... \033[0m] Init RTC (time)\n");
    init_rtc_time();
    printf("[\033[92m wow \033[0m] RTC done\n");
    printf("[\033[96m ... \033[0m] Mounting ExFAT\n");
    if (fs_mount()) {
        printf("[\033[91m bad \033[0m] Mount failed\n");
        halt();
    }
    printf("[\033[92m wow \033[0m] Mount done\n");
    printf("[\033[96m ... \033[0m] Init System Calls\n");
    init_syscalls();
    printf("[\033[92m wow \033[0m] System Calls done\n");

    printf("\033[96mWindogeOS has booted. \033[0m\n");
    printf("\033[96mUh, now what?\n\033[0m");
    sti();

    users_init();
    temp();
    user_add("wow", "wow", 1);
    if (!fs_exists(".windoge")) {
        create_account_setup();
    } else {
        login_screen();
    }

    clear();
    run("/programs/test", 32768);
    run("/programs/sh", 32768);
    run("/system/dogeshell", 32768);
    halt();
}
