#include "multiboot.h"
#include "../helper/dogeio.h"
#include "../helper/time.h"
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
        dogeio_print("[Warning] Multiboot flags invalid\n");
    } else {
        dogeio_print("[Wow] Multiboot flags valid.\n");
    }

    dogeio_print("Booting WindogeOS... pls wait, such patiance.\n");

    return 1;
}

void record_boot_time(char* boot_buffer) {
    char* raw = time_get_raw();

    int i = 0;
    while (raw[i] != '\0') {
        boot_buffer[i] = raw[i];
        i++;
    }
    boot_buffer[i] = '\0';
}
