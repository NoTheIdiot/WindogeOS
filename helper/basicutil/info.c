#include <stdint.h> 
#include <dogeio.h>
#include <string.h>
#include <basicutil.h>
#include <system.h>
#include <boot/limine.h>

extern volatile struct limine_memmap_request memmap_request;

char* windoge_version = "WindogeOS v0.2-Beta4";
char current_user[64];
char* dogeshell_version = "Dogeshell v2.1";

const char* doge_ascii[22] = {
    "                 ;i.",
    "                  M$L                    .;i.          ",
    "                  M$Y;                .;iii;;.         ",
    "                 ;$YY$i._           .iiii;;;;;         ",
    "                .iiiYYYYYYiiiii;;;;i;iii;; ;;;         ",
    "              .;iYYYYYYiiiiiiYYYiiiiiii;;  ;;;         ",
    "           .YYYY$$$$YYYYYYYYYYYYYYYYiii;; ;;;;         ",
    "         .YYY$$$$$$YYYYYY$$$$iiiY$$$$$$$ii;;;;         ",
    "        :YYYF`,  TYYYYY$$$$$YYYYYYYi$$$$$iiiii;        ",
    "       Y$MM: \\\\  :YYYY$$P\"````\"T$YYMMMMMMMMiiYY.     ",
    "     `.;\\\\[M\\\\]b.,dYY\\\\[Yi; .( .YYMMM\\\\]\\\\$MMMMYY      ",
    "    .._MMMMM!YYYYYYYYYi;.`\\\\\"  .;iiMMM$MMMMMMMYY      ",
    "    ._$MMMP` ```\"\"4$$$$$iiiiiiii$MMMMMMMMMMMMMY;     ",
    "     MMMM$:       :$$$$$$$MMMMMMMMMMM$$MMMMMMMYYL      ",
    "    :MMMM$$.    .;PPb$$$$MMMMMMMMMM$$$$MMMMMMiYYU:     ",
    "     iMM$$;;: ;;;;i$$$$$$$MMMMM$$$$MMMMMMMMMMYYYYY     ",
    "     `$$$$i .. ``:iiii!*\"``.$$$$$$$$$MMMMMMM$YiYYY    ",
    "      :Y$$iii;;;.. ` ..;;i$$$$$$$$$MMMMMM$$YYYYiYY:    ",
    "       :$$$$$iiiiiii$$$$$$$$$$$MMMMMMMMMMYYYYiiYYYY.   ",
    "        `$$$$$$$$$$$$$$$$$$$$MMMMMMMM$YYYYYiiiYYYYYY   ",
    "         YY$$$$$$$$$$$$$$$$MMMMMMM$$YYYiiiiiiYYYYYYY   ",
    "        :YYYYYY$$$$$$$$$$$$$$$$$$YYYYYYYiiiiYYYYYYi'   "
};

char* cpuid(void) {
    static char brand[49];
    uint32_t regs[4];

    for (uint32_t i = 0; i < 3; i++) {
        __asm__ volatile (
            "cpuid"
            : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
            : "a" (0x80000002 + i)
        );

        for (uint32_t j = 0; j < 4; j++) {
            brand[(i * 16) + (j * 4) + 0] = (char)(regs[j] & 0xFF);
            brand[(i * 16) + (j * 4) + 1] = (char)((regs[j] >> 8) & 0xFF);
            brand[(i * 16) + (j * 4) + 2] = (char)((regs[j] >> 16) & 0xFF);
            brand[(i * 16) + (j * 4) + 3] = (char)((regs[j] >> 24) & 0xFF);
        }
    }

    brand[48] = '\0';
    return brand;
}

uint64_t get_ram(void) {
    if (memmap_request.response == NULL) {
        return 0; 
    }

    uint64_t total_ram_bytes = 0;
    uint64_t entries = memmap_request.response->entry_count;

    for (uint64_t i = 0; i < entries; i++) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            total_ram_bytes += entry->length;
        }
    }

    return total_ram_bytes;
}

uint64_t get_ram_end_address(void) {
    if (memmap_request.response == NULL) {
        return 0; 
    }

    uint64_t highest_address = 0;
    uint64_t entries = memmap_request.response->entry_count;

    for (uint64_t i = 0; i < entries; i++) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t entry_end = entry->base + entry->length;
            if (entry_end > highest_address) {
                highest_address = entry_end;
            }
        }
    }

    return highest_address;
}


__attribute__((section(".limine_requests"))) static const uint8_t code_marker_start = 0;
__attribute__((section(".bss")))             static uint8_t       code_marker_end;

uint64_t get_used_ram(void) {
    uint64_t binary_size = (uint64_t)&code_marker_end - (uint64_t)&code_marker_start;
    return binary_size + (TERMINAL_ROWS * TERMINAL_COLS);
}


void system_fetch() {
    char ram_str[32]; 
    char ram_used_str[32];
    char storage_buf[32];
    uint32_t old3 = dogeio_text_color;
    
    uint32_t total_clusters = 0;
    uint32_t free_clusters = 0;
    int metrics_status = exfat_get_space_metrics(&total_clusters, &free_clusters);

    for (int i = 0; i < 22; i++) {
        dogeio_text_color_change(0xE1B16C);
        dogeio_text_print(doge_ascii[i]);

        dogeio_text_color_change(old3);
        switch (i) {
            case 1:
                dogeio_text_print(current_user);
                dogeio_text_print(" / ");
                dogeio_text_println("wow-computer");
                break;
            case 2:
                dogeio_text_println("-----------------------------");
                break;
            case 3:
                dogeio_text_print("Such User: ");
                dogeio_text_println(current_user);
                break;
            case 4:
                dogeio_text_print("Version: ");
                dogeio_text_println(windoge_version);
                break;
            case 5:
                dogeio_text_print("Shell: ");
                dogeio_text_println(dogeshell_version);
                break;
            case 6:
                dogeio_text_print("CPU: ");
                dogeio_text_println(cpuid());
                break;
            case 7:
                dogeio_text_print("RAM: ");
                str_itoa((int)(get_used_ram() / 1024 / 1024), ram_used_str);
                dogeio_text_print(ram_used_str);
                dogeio_text_print(" MB");
                dogeio_text_print(" / ");
                str_itoa((int)(get_ram() / 1024 / 1024), ram_str);
                dogeio_text_print(ram_str);
                dogeio_text_println(" MB");
                break;
            case 8:
                dogeio_text_print("Storage: ");
                if (metrics_status == 0 && total_clusters > 0) {
                    uint32_t used_clusters = total_clusters - free_clusters;
                    str_itoa((int)used_clusters, storage_buf);
                    dogeio_text_print(storage_buf);
                    dogeio_text_print(" / ");
                    str_itoa((int)total_clusters, storage_buf);
                    dogeio_text_print(storage_buf);
                    dogeio_text_println(" Clusters");
                } else {
                    dogeio_text_println("Error reading exFAT");
                }
                break;
            case 9:
                if (metrics_status == 0 && total_clusters > 0) {
                    uint32_t used_clusters = total_clusters - free_clusters;
                    uint32_t percentage = (used_clusters * 100U) / total_clusters;
                    
                    dogeio_text_print("Disk Map: [");
                    uint32_t bar_width = 12;
                    uint32_t filled = (percentage * bar_width) / 100U;
                    for (uint32_t j = 0; j < bar_width; j++) {
                        if (j < filled) {
                            dogeio_text_print("=");
                        } else {
                            dogeio_text_print(".");
                        }
                    }
                    dogeio_text_print("] ");
                    str_itoa((int)percentage, storage_buf);
                    dogeio_text_print(storage_buf);
                    dogeio_text_println("%");
                } else {
                    dogeio_text_println("");
                }
                break;
            case 10:
                dogeio_text_background_change(COLOR_RED);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_ORANGE);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_YELLOW);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_GREEN);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BLUE);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_CYAN);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_MAGENTA);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BLACK);
                dogeio_text_println("");
                break;
            case 11:
                dogeio_text_print(" ");
                dogeio_text_background_change(COLOR_BRIGHT_RED);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BRIGHT_ORANGE);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BRIGHT_YELLOW);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BRIGHT_GREEN);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BRIGHT_BLUE);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BRIGHT_CYAN);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BRIGHT_MAGENTA);
                dogeio_text_print("   ");
                dogeio_text_background_change(COLOR_BLACK);
                dogeio_text_println("");
                break;
                
            default:
                dogeio_text_println("");
        }
    }
}
