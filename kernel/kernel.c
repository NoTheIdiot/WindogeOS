// include files
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

extern int such_check_multiboot(uint32_t magic, multiboot_info_t* mbi);
extern void doge_shell();
extern void record_boot_time(char* boot_buffer);
extern char* such_windoge_version;
extern char* such_windoge_version_short;
char* boot_time = "";
uint8_t rtc_init = 2;

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {

    // i just wasted a whole day trying to add WAITING?
    // always set this first
    //idt_init();
    //set_idt_gate(40, (uint32_t)rtc_isr);

    dogeio_init_graphics_from_multiboot(mbi);

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

    // time_rtc_init(0x0F);
    record_boot_time(boot_time);
    mem_init(mbi);
    file_init_filesystem();
    such_check_multiboot(magic, mbi);
    dogeio_print("Welcome to WindogeOS! ");
    dogeio_println(such_windoge_version_short);
    doge_shell();
}
