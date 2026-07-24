#include <stdint.h> 
#include <basicutil.h>
#include <boot/limine.h>

extern volatile struct limine_memmap_request memmap_request;

const char* doge_ascii[22] = {
    "                 ;i.",
    "                  M$L                    .;i.",
    "                  M$Y;                .;iii;;.",
    "                 ;$YY$i._           .iiii;;;;;",
    "                .iiiYYYYYYiiiii;;;;i;iii;; ;;;",
    "              .;iYYYYYYiiiiiiYYYiiiiiii;;  ;;;",
    "           .YYYY$$$$YYYYYYYYYYYYYYYYiii;; ;;;;",
    "         .YYY$$$$$$YYYYYY$$$$iiiY$$$$$$$ii;;;;",
    "        :YYYF`,  TYYYYY$$$$$YYYYYYYi$$$$$iiiii;",
    "       Y$MM: \\\\  :YYYY$$P\"````\"T$YYMMMMMMMMiiYY.",
    "     `.;\\[M\\]b.,dYY\\[Yi; .(     .YYMMM\\]\\$MMMMYY",
    "    .._MMMMM!YYYYYYYYYi;.`\"  .;iiMMM$MMMMMMMYY",
    "    ._$MMMP` ```\"\"4$$$$$iiiiiiii$MMMMMMMMMMMMMY;",
    "     MMMM$:       :$$$$$$$MMMMMMMMMMM$$MMMMMMMYYL",
    "    :MMMM$$.    .;PPb$$$$MMMMMMMMMM$$$$MMMMMMiYYU:",
    "     iMM$$;;: ;;;;i$$$$$$$MMMMM$$$$MMMMMMMMMMYYYYY",
    "     `$$$$i .. ``:iiii!*\"``.$$$$$$$$$MMMMMMM$YiYYY",
    "      :Y$$iii;;;.. ` ..;;i$$$$$$$$$MMMMMM$$YYYYiYY:",
    "       :$$$$$iiiiiii$$$$$$$$$$$MMMMMMMMMMYYYYiiYYYY.",
    "        `$$$$$$$$$$$$$$$$$$$$MMMMMMMM$YYYYYiiiYYYYYY",
    "         YY$$$$$$$$$$$$$$$$MMMMMMM$$YYYiiiiiiYYYYYYY",
    "        :YYYYYY$$$$$$$$$$$$$$$$$$YYYYYYYiiiiYYYYYYi'"
};

char* cpuid(void) {
    static char vendor[13]; 
    uint32_t eax, ebx, ecx, edx;

    __asm__ volatile (
        "cpuid"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (0)
    );

    vendor[0]  = (char)(ebx & 0xFF);
    vendor[1]  = (char)((ebx >> 8) & 0xFF);
    vendor[2]  = (char)((ebx >> 16) & 0xFF);
    vendor[3]  = (char)((ebx >> 24) & 0xFF);

    vendor[4]  = (char)(edx & 0xFF);
    vendor[5]  = (char)((edx >> 8) & 0xFF);
    vendor[6]  = (char)((edx >> 16) & 0xFF);
    vendor[7]  = (char)((edx >> 24) & 0xFF);

    vendor[8]  = (char)(ecx & 0xFF);
    vendor[9]  = (char)((ecx >> 8) & 0xFF);
    vendor[10] = (char)((ebx >> 16) & 0xFF);
    vendor[11] = (char)((ecx >> 24) & 0xFF);
    
    vendor[12] = '\0'; 

    return vendor; 
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