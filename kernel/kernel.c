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

// FIXED: Changed from extern void to extern int to match Option A's return behavior!
extern int windoge_boot_manager(); 

extern int friendly_mode();
extern char* such_windoge_version;
extern char* such_windoge_version_short;

// FIX PREVENTING UB: Changed to an explicit array size to prevent undefined behavior when writing strings
char boot_time[64] = ""; 
uint8_t rtc_init = 2;

static uint8_t file_text_buffer[4096];

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {

    // init vbe
    dogeio_init_vbe_from_multiboot(mbi);

    // debug multiboot information
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

    // clear the display
    dogeio_clear_screen();
    // record the boot time
    record_boot_time(boot_time);
    // initialize fat32 at sector 0
    fat32_init(0); 
    // initialize memory tracking
    mem_init(mbi);

    // boot manager, always must be and should be after the hardware init.
    int friendly = windoge_boot_manager();

    // check if multiboot is valid
    such_check_multiboot(magic, mbi);

    dogeio_println("Press Enter to start WindogeOS.");
    char enter[4];
    dogeio_input(enter, 4, DOGE_COLOR);
    dogeio_clear_screen();
    dogeio_clear_screen(); 

    if (friendly == 1) {
        friendly_mode();
    } 

    // start the operating system 
    dogeio_println("**********************************************************************");
    dogeio_println("** Welcome to WindogeOS!");
    dogeio_print("** Version ");
    dogeio_println(such_windoge_version);
    dogeio_println("**********************************************************************");

    // finish the cluster for the text file
    uint32_t text_file_cluster = find_dir_cluster_by_path("/doge.txt");
    if (text_file_cluster != (uint32_t)-1) {
        for (int i = 0; i < 4096; i++) {
            file_text_buffer[i] = '\0';
        }
        read_cluster((uint32_t)text_file_cluster, file_text_buffer);
        dogeio_println((char*)file_text_buffer);
    }

    dogeio_println("");
    doge_shell(0);
}
