#include <stdint.h> 
#include <dogeio.h>
#include <string.h>
#include <basicutil.h>
#include <system.h>
#include <boot/limine.h>

extern volatile struct limine_memmap_request memmap_request;

char* windoge_version = "WindogeOS v0.0.3";

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

void system_fetch() {
    char ram_str[32]; 

    for (int i = 0; i < 22; i++) {
        dogeio_text_print(doge_ascii[i]);

        switch (i) {
            case 1:
                dogeio_text_println("wow/computer");
                break;
            case 2:
                dogeio_text_println("-----------------------------");
                break;
			case 3:
				dogeio_text_print("Such User: ");
				dogeio_text_println("wow");
				break;
			case 4:
				dogeio_text_print("Version: ");
				dogeio_text_println(windoge_version);
				break;
            case 5:
                dogeio_text_print("CPU: ");
                dogeio_text_println(cpuid());
                break;
            case 6:
                dogeio_text_print("RAM: ");
                string_itoa((int)(get_ram() / 1024 / 1024), ram_str); 
                dogeio_text_print(ram_str);
                dogeio_text_println(" MB");
                break;
			case 7:
				dogeio_text_print("HDD: ");
				dogeio_text_print(system_file_amount_string());
				dogeio_text_println(" / 32 files");
				break;
            default:
                dogeio_text_println(""); 
                break;
        }
    }
}
