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

extern uint32_t find_dir_cluster_by_path(const char* path);
extern void read_cluster(uint32_t cluster, uint8_t *buffer);

static uint8_t file_text_buffer[4096];

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

    dogeio_clear_screen();
    record_boot_time(boot_time);
    fat32_init(0); 
    mem_init(mbi);
    such_check_multiboot(magic, mbi);
    dogeio_print("Welcome to WindogeOS! ");
    dogeio_println(such_windoge_version_short);

    uint32_t text_file_cluster = find_dir_cluster_by_path("/doge.txt");
    if (text_file_cluster != (uint32_t)-1) {
        for (int i = 0; i < 4096; i++) {
            file_text_buffer[i] = '\0';
        }
        read_cluster((uint32_t)text_file_cluster, file_text_buffer);
        dogeio_println((char*)file_text_buffer);
    }

    dogeio_println("");
    doge_shell();
}
