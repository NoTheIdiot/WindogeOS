#include <limine.h>
#include <system.h>
#include <dogeio.h>
#include <string.h>

static struct limine_module_response *mod_res = NULL;

int system_file_init(struct limine_module_response *response) {
    if (!response) {
        return -1;
    }
    mod_res = response;
    return 0;
}

int system_file_list_directory(void) {
    if (!mod_res) {
        return -1;
    }

    dogeio_text_println("Attributes | Size (Bytes) | Name");
    dogeio_text_println("----------------------------------------");

    for (uint64_t i = 0; i < mod_res->module_count; i++) {
        struct limine_file *file = mod_res->modules[i];
        
        dogeio_text_print("-R----     | ");
        
        char size_str[32];
        string_itoa((uint32_t)file->size, size_str);
        dogeio_text_print(size_str);
        
        dogeio_text_print(" | ");
        dogeio_text_println(file->path);
    }

    return 0;
}
