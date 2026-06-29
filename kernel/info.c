#include <system.h>
#include <string.h>
#include <dogeio.h>
#include <stdint.h>
#include <limine.h>
#include <system.h>

extern volatile struct limine_memmap_request memmap_request;

void system_info_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf));
}

void system_info_ram(char *out_buffer) {
    if (memmap_request.response == NULL) {
        dogeio_text_println("not wow: limine memmap not found.");
        out_buffer[0] = '\0';
        return;
    }

    uint64_t total_bytes = 0;
    struct limine_memmap_response *response = memmap_request.response;

    for (uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry *entry = response->entries[i];
        total_bytes += entry->length;
    }

    uint64_t total_mb = total_bytes / (1024 * 1024);
    string_itoa(total_mb, out_buffer);
}

void system_info_cpu(char *out_vendor) {
    uint32_t eax, ebx, ecx, edx;
    system_info_cpuid(0, &eax, &ebx, &ecx, &edx);

    *(uint32_t *)(out_vendor)     = ebx;
    *(uint32_t *)(out_vendor + 4) = edx;
    *(uint32_t *)(out_vendor + 8) = ecx;
    out_vendor[12] = '\0';
}
