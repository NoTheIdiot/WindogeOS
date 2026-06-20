#include <stdint.h>
#include <stddef.h>
#include "dogeio.h"
#include "dogeio_vbe.h"
#include "vbe.h"
#include "string.h"
#include "ports.h"
#include "time.h"
#include "consts.h"
#include "multiboot.h"
#include "info.h"
#include "serial.h"
#include "file.h"
#include "idt.h"
#include "dogeshell.h"

extern int such_check_multiboot(uint32_t magic, multiboot_info_t* mbi);
extern void record_boot_time(char* boot_buffer);
extern int windoge_boot_manager(void); 
extern int friendly_mode(void);

extern char* such_windoge_version;
extern char* such_windoge_version_short;

char boot_time[64] = ""; 
uint8_t rtc_init = 2;

// Placed in .bss away from the critical execution stack region
static uint8_t file_text_buffer[4096];

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    // 1. HARDWARE & MEMORY INITIALIZATION FIRST
    // Initialize low-level memory layout maps before touching driver layers
    mem_init(mbi);

    // 2. RUN GRUB MULTIBOOT HANDSHAKE EVALUATION
    such_check_multiboot(magic, mbi);

    // 3. INITIALIZE VBE GRAPHICS NOW THAT MEMORY MAPS ARE SAFE
    dogeio_init_vbe_from_multiboot(mbi);

    // Debugging verification pipeline output
    serial_write_string("[DEBUG] mbi.flags=");
    serial_write_hex(mbi->flags);
    serial_write_string(" framebuf=");
    serial_write_hex((uint32_t)(mbi->framebuffer_addr & 0xFFFFFFFF));
    serial_write_string(" width=");
    serial_write_hex(mbi->framebuffer_width);
    serial_write_string(" height=");
    serial_write_hex(mbi->framebuffer_height);
    serial_write_string(" bpp=");
    serial_write_hex(mbi->framebuffer_bpp);
    serial_write_string(" vbe=");
    serial_write_hex(vbe_initialized);
    serial_write_string("\n");
    dogeio_clear_screen();

    record_boot_time(boot_time);
    fat32_init(0); // Targets sector 0 for raw, unpartitioned loopback storage matrix mappings

    int friendly = windoge_boot_manager();

    dogeio_println("Press Enter to start WindogeOS.");
    char enter[4];
    dogeio_input(enter, 4, DOGE_COLOR);
    dogeio_clear_screen();

    if (friendly == 1) {
        friendly_mode();
    } 

    // 6. WELCOME SPLASH LAYER OUTPUT
    dogeio_println("**********************************************************************");
    dogeio_println("** Welcome to WindogeOS!");
    dogeio_print("** Version ");
    dogeio_println(such_windoge_version);
    dogeio_println("**********************************************************************");

    // High-speed block reset
    for (int i = 0; i < 4096; i++) {
        file_text_buffer[i] = '\0';
    }

    // Correct file extraction using your standard high-level FAT32 filesystem user-space routing APIs
    if (fat32_read_file("/", "DOGE    ", "TXT", file_text_buffer) != -1) {
        dogeio_println((char*)file_text_buffer);
    } else {
        serial_write_string("[ERROR] Failed to extract boot splash text file asset\n");
    }

    dogeio_println("");
    
    doge_shell(0);
}
