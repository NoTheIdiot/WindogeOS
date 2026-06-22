#include "multiboot.h"
#include "dogeio.h"
#include "string.h"
#include "time.h"
#include <stdint.h>

int such_check_multiboot(uint32_t magic, multiboot_info_t* mbi) {
    dogeio_print("[Note] Check such grub multiboot header...\n");
    
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        dogeio_print("[Not Wow] Multiboot magic value invalid.\n");
        while (1) { __asm__("hlt"); }
    } else {
        dogeio_print("[Wow] Multiboot magic value valid.\n");
    }

    if (!(mbi->flags & MBI_FLAG_MMAP)) {
        dogeio_print("[Warning] Multiboot memory map flag missing!\n");
    } else {
        dogeio_print("[Wow] Multiboot memory map valid.\n");
    }

    if (mbi->flags & (1 << 11)) {
        dogeio_print("[Wow] Multiboot knows VBE graphics!\n");
    } else {
        dogeio_print("[Warning] Multiboot did NOT initialize graphics mode.\n");
    }

    dogeio_print("Booting WindogeOS... pls wait, such patience.\n");
    return 1;
}


void record_boot_time(char* boot_buffer) {
    char* raw = time_get_raw();
    string_strcpy(boot_buffer, raw);
}
