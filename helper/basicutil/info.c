#include <stdint.h> 
#include <dogeio.h>
#include <string.h>
#include <basicutil.h>
#include <system.h>
#include <boot/limine.h>

extern volatile struct limine_memmap_request memmap_request;

char current_user[64];
const char* dogeshell_version = "Dogeshell v2.3";
char* windoge_version = "WindogeOS v0.3-Beta1";

#if defined(__x86_64__) || defined(_M_X64) 
    const char* windoge_arch = "x86_64 (Intel/AMD)";
#elif defined(__aarch64__) || defined (_M_ARM64)
    const char* windoge_arch = "ARM64/Aarch64";
#else
    const char* windoge_arch = "Unknown";
#endif

const char* doge_ascii[22] = {
    "                 ;i.                                   ",
    "                  M$L                    .;i.          ",
    "                  M$Y;                .;iii;;.         ",
    "                 ;$YY$i._           .iiii;;;;;         ",
    "                .iiiYYYYYYiiiii;;;;i;iii;; ;;;         ",
    "              .;iYYYYYYiiiiiiYYYiiiiiii;;  ;;;         ",
    "           .YYYY$$$$YYYYYYYYYYYYYYYYiii;; ;;;;         ",
    "         .YYY$$$$$$YYYYYY$$$$iiiY$$$$$$$ii;;;;         ",
    "        :YYYF`,  TYYYYY$$$$$YYYYYYYi$$$$$iiiii;        ",
    "       Y$MM: \\\\  :YYYY$$P\"````\"T$YYMMMMMMMMiiYY.       ",
    "     `.;\\\\[M\\\\]b.,dYY\\\\[Yi; .( .YYMMMMM$MMMMMMYY       ",
    "    .._MMMMM!YYYYYYYYYi;.`\\\\\"  .;iiMM$$MMMMMMMYY       ",
    "    ._$MMMP` ```\"\"4$$$$$iiiiiiii$MMMM$MMMMMMMMY;       ",
    "     MMMM$:       :$$$$$$$MMMMMMMMMMM$$MMMMMMMYYL      ",
    "    :MMMM$$.    .;PPb$$$$MMMMMMMMMM$$$$MMMMMMiYYU:     ",
    "     iMM$$;;: ;;;;i$$$$$$$MMMMM$$$$MMMMMMMMMMYYYYY     ",
    "     `$$$$i .. ``:iiii!*\"``.$$$$$$$$$MMMMMMM$YiYYY     ",
    "      :Y$$iii;;;.. ` ..;;i$$$$$$$$$MMMMMM$$YYYYiYY:    ",
    "       :$$$$$iiiiiii$$$$$$$$$$$MMMMMMMMMMYYYYiiYYYY.   ",
    "        `$$$$$$$$$$$$$$$$$$$$MMMMMMMM$YYYYYiiiYYYYYY   ",
    "         YY$$$$$$$$$$$$$$$$MMMMMMM$$YYYiiiiiiYYYYYYY   ",
    "        :YYYYYY$$$$$$$$$$$$$$$$$$YYYYYYYiiiiYYYYYYi'   "
};

const char* doge_ascii_full[24] = {
    "================================================================================================================",
"                 ;i.                                    ",
"                  M$L                    .;i.           ",
"                  M$Y;                .;iii;;.          ",
"                 ;$YY$i._           .iiii;;;;;          ",
"                .iiiYYYYYYiiiii;;;;i;iii;; ;;;          ",
"              .;iYYYYYYiiiiiiYYYiiiiiii;;  ;;;          ",
"           .YYYY$$$$YYYYYYYYYYYYYYYYiii;; ;;;;        #     # ### #     # ######  #######  #####  #######",
"         .YYY$$$$$$YYYYYY$$$$iiiY$$$$$$$ii;;;;        #  #  #  #  ##    # #     # #     # #     # #      ",
"        :YYYF`,  TYYYYY$$$$$YYYYYYYi$$$$$iiiii;       #  #  #  #  # #   # #     # #     # #       #       ",
"        Y$MM: \\  :YYYY$$P\"````\"T$YYMMMMMMMMiiYY.      #  #  #  #  #  #  # #     # #     # #  #### #####   ",
"     `.;$$M$$b.,dYY$$Yi; .(     .YYMMM$$$MMMMYY       #  #  #  #  #   # # #     # #     # #     # #      ",
"   .._$MMMMM$!YYYYYYYYYi;.`\"  .;iiMMM$MMMMMMMYY       #  #  #  #  #    ## #     # #     # #     # #       ",
"    ._$MMMP` ```\"\"4$$$$$iiiiiiii$MMMMMMMMMMMMMY;       ## ##  ### #     # ######  #######  #####  #######",
"     MMMM$:       :$$$$$$$MMMMMMMMMMM$$MMMMMMMYYL     ================  ",
"    :MMMM$$.    .;PPb$$$$MMMMMMMMMM$$$$MMMMMMiYYU:    v0.3.0-Beta1",
"     iMM$$;;: ;;;;i$$$$$$$MMMMM$$$$MMMMMMMMMMYYYYY      ",
"     `$$$$i .. ``:iiii!*\"``.$$$$$$$$$MMMMMMM$YiYYY      ",
"      :Y$$iii;;;.. ` ..;;i$$$$$$$$$MMMMMM$$YYYYiYY:     ",
"       :$$$$$iiiiiii$$$$$$$$$$$MMMMMMMMMMYYYYiiYYYY.    ",
"        `$$$$$$$$$$$$$$$$$$$$MMMMMMMM$YYYYYiiiYYYYYY    ",
"         YY$$$$$$$$$$$$$$$$MMMMMMM$$YYYiiiiiiYYYYYYY    ",
"        :YYYYYY$$$$$$$$$$$$$$$$$$YYYYYYYiiiiYYYYYYi'    ",
"================================================================================================================"
};

char* cpuid_string(void) {
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
    if (memmap_request.response == NULL || memmap_request.response->entry_count == 0) {
        return 0; 
    }

    uint64_t entries = memmap_request.response->entry_count;
    uint64_t type_counts[8] = {0}; 

    for (uint64_t i = 0; i < entries; i++) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (entry->type < 8) {
            type_counts[entry->type] += entry->length;
        }
    }

    return (type_counts[0] + type_counts[7]) / 1024;
}

char* get_user(void) {
    return current_user;
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
    uint32_t old3 = dogeio_color;
    
    uint32_t total_clusters = 0;
    uint32_t free_clusters = 0;
    int metrics_status = exfat_get_space_metrics(&total_clusters, &free_clusters);

    for (int i = 0; i < 22; i++) {
        dogeio_color_change(0xE1B16C);
        dogeio_print(doge_ascii[i]);

        dogeio_color_change(old3);
        switch (i) {
            case 1:
                dogeio_print(current_user);
                dogeio_print(" / ");
                dogeio_println("wow-computer");
                break;
            case 2:
                dogeio_println("-----------------------------");
                break;
            case 3:
                dogeio_print("Version: ");
                dogeio_println(windoge_version);
                break;
            case 4:
                dogeio_print("Arch: ");
                dogeio_println(windoge_arch);
                break;
            case 5:
                dogeio_print("Such User: ");
                dogeio_println(current_user);
                break;
            case 6:
                dogeio_print("Shell: ");
                dogeio_println(dogeshell_version);
                break;
            case 7:
                dogeio_print("CPU: ");
                dogeio_println(cpuid_string());
                break;
            case 8:
                dogeio_print("RAM: ");
                
                uint64_t ram_used_mib = get_used_ram() / 1024 / 1024;
                uint64_t ram_total_kb = get_ram();

                str_kutoa(ram_used_mib, ram_used_str);
                dogeio_print(ram_used_str);
                dogeio_print(" MB / ");
                
                if (ram_total_kb >= 1024) {
                    str_kutoa(ram_total_kb / 1024, ram_str);
                    dogeio_print(ram_str);
                    dogeio_println(" MB");
                } else {
                    str_kutoa(ram_total_kb, ram_str);
                    dogeio_print(ram_str);
                    dogeio_println(" KB");
                }
                break;
            case 9:
                dogeio_print("Storage: ");
                if (metrics_status == 0 && total_clusters > 0) {
                    uint32_t used_clusters = total_clusters - free_clusters;
                    str_itoa((int)used_clusters, storage_buf);
                    dogeio_print(storage_buf);
                    dogeio_print(" / ");
                    str_itoa((int)total_clusters, storage_buf);
                    dogeio_print(storage_buf);
                    dogeio_println("Clusters");
                } else {
                    dogeio_println("Error reading exFAT");
                }
                break;
            case 10: {
                char width[32];
                char height[32];

                dogeio_print("Display: ");

                str_itoa(dogeio_screen_width(), width);
                str_itoa(dogeio_screen_height(), height);

                dogeio_print(width);
                dogeio_print("x");
                dogeio_println(height);
                break;
            }
            case 12:
                dogeio_background_change(COLOR_RED);
                dogeio_print("   ");
                dogeio_background_change(COLOR_ORANGE);
                dogeio_print("   ");
                dogeio_background_change(COLOR_YELLOW);
                dogeio_print("   ");
                dogeio_background_change(COLOR_GREEN);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BLUE);
                dogeio_print("   ");
                dogeio_background_change(COLOR_CYAN);
                dogeio_print("   ");
                dogeio_background_change(COLOR_MAGENTA);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BLACK);
                dogeio_println("");
                break;
            case 13:
                dogeio_background_change(COLOR_BRIGHT_RED);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BRIGHT_ORANGE);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BRIGHT_YELLOW);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BRIGHT_GREEN);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BRIGHT_BLUE);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BRIGHT_CYAN);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BRIGHT_MAGENTA);
                dogeio_print("   ");
                dogeio_background_change(COLOR_BLACK);
                dogeio_println("");
                break;
            default:
                dogeio_println("");
        }
    }
}
