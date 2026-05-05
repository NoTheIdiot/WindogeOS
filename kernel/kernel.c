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

static inline void serial_write_char(char c) {
    while ((ports_inb(0x3F8 + 5) & 0x20) == 0);
    ports_outb(0x3F8, (uint8_t)c);
}

static void serial_write_string(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        serial_write_char(str[i]);
    }
}

static void serial_write_hex(uint32_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_write_string("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uint8_t digit = (value >> shift) & 0xF;
        serial_write_char(hex[digit]);
    }
}

extern int such_check_multiboot(uint32_t magic, multiboot_info_t* mbi);
extern void record_boot_time();
extern char* such_windoge_version;

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
    
    char uptime_buffer[64];
    char command[64];
    char print_input[64];
    char cpu_buffer[64];
    
    // If graphics mode is available, use it; otherwise fall back to text mode
    if (vbe_initialized) {
        dogeio_println_graphics("Welcome to WindogeOS - Graphics Mode!");
        dogeio_println_graphics(such_windoge_version);
    } else {
        // Fall back to text mode
        dogeio_clear_screen();
        such_check_multiboot(magic, mbi);
        record_boot_time(uptime_buffer);
        dogeio_print("[Note] Such time is ");
        dogeio_print(uptime_buffer);
        dogeio_print("\n");

        dogeio_print("Welcome to WindogeOS! ");
        dogeio_println(such_windoge_version);
    }

    while (1) {
        dogeio_print("windoge~# ");
        dogeio_input(command, 64, DOGE_COLOR);

        if (string_strcmp(command, "print") == 0 || string_strcmp(command, "bark") == 0) {
            dogeio_print("print> ");
            dogeio_input(print_input, 64, DOGE_COLOR);
            dogeio_print(print_input);
            dogeio_print("\n");

        } else if (string_strcmp(command, "shutdown") == 0) {
            dogeio_clear_screen();
            dogeio_print("Such shutdown, mauch goodbye.\n");
            while (1) {__asm__("hlt");}
        
        } else if (string_strcmp(command, "ver") == 0 || string_strcmp(command, "such-version") == 0) {
            dogeio_println(such_windoge_version);

        } else if (string_strcmp(command, "clear") == 0) {
            dogeio_clear_screen();

        } else if (string_strcmp(command, "date") == 0 || string_strcmp(command, "time") == 0) {
            time_update_time();
            time_show();
            dogeio_print("\n");
                
        } else if (string_strcmp(command, "sysinfo") == 0) {
            uint32_t ram_amount = info_get_ram_amount(mbi);
            char ram_amount_string[64];
            string_itoa(ram_amount, ram_amount_string);
            time_update_time();

            dogeio_print("windoge.human\n");
            dogeio_print("-----------------------\n");
            dogeio_print("Version: ");
            dogeio_println(such_windoge_version);
            dogeio_print("CPU: ");
            info_get_cpu_name(cpu_buffer);
            dogeio_print(cpu_buffer);
            dogeio_print("\n");
            dogeio_print("Usable RAM: ");
            dogeio_print(ram_amount_string);
            dogeio_print("MB");
            dogeio_print("\n");
            dogeio_print("Time: ");
            time_show();
            dogeio_print("\n");
            dogeio_print("Boot Time: ");
            dogeio_print(uptime_buffer);
            dogeio_print("\n");

        } else {
            dogeio_print("command not found.\n");
        }
    }
}
