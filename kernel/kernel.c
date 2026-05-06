// include files
#include <stdint.h>
#include <stddef.h>
#include "../helper/dogeio.h"
#include "../helper/dogeio_graphics.h"
#include "../helper/vbe.h"
#include "../helper/string.h"
#include "../helper/ports.h"
#include "../helper/time.h"
#include "consts.h"
#include "multiboot.h"
#include "info.h"
#include "serial.h"

extern int such_check_multiboot(uint32_t magic, multiboot_info_t* mbi);
extern void doge_shell(uint32_t magic, multiboot_info_t* mbi);
extern void record_boot_time();
extern char* such_windoge_version;
extern char* such_windoge_version_short;

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {

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
    
    such_check_multiboot(magic, mbi);
    doge_shell(magic, mbi);
}