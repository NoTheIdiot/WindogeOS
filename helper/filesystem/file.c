#include <limine.h>
#include <system.h>
#include <dogeio.h>
#include <string.h>

#define MAX_LINES 1024
#define MAX_CHARS 256

static uint32_t range = 0;     
static uint32_t rangehigh = 0; 
char output[MAX_LINES][MAX_CHARS]; // output

static struct limine_module_response *mod_res = NULL;
extern volatile struct limine_module_request module_request;

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

int system_file_readfile(const char *filename) {
    if (module_request.response == NULL || module_request.response->module_count == 0) {
        dogeio_duolog("Error: No boot modules loaded in memory.");
        return -1;
    }

    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file *file = module_request.response->modules[i];
        
        const char *haystack = file->path; 
        const char *needle = filename;
        uint32_t match = 0;

        while (*haystack != '\0') {
            const char *h = haystack;
            const char *n = needle;

            while (*n != '\0' && *h == *n) {
                h++;
                n++;
            }

            if (*n == '\0' && *h == '\0') {
                match = 1;
                break;
            }
            haystack++;
        }

        if (match == 1) {
            char *file_data = (char *)file->address;

            for (uint64_t byte_idx = 0; byte_idx < file->size; byte_idx++) {
                dogeio_text_printchar(file_data[byte_idx]);
            }
            
            dogeio_text_printchar('\n');
            return 0; 
        }
    }

    return -2;
}

int system_file_load(const char *filename) {
    if (module_request.response == NULL) return -1;

    range = 0;
    rangehigh = 0;

    for (int i = 0; i < MAX_LINES; i++) {
        for (int j = 0; j < MAX_CHARS; j++) {
            output[i][j] = '\0';
        }
    }

    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file *file = module_request.response->modules[i];
        
        if (string_strcmp(file->path, filename) != 0) {
            continue;
        }

        const char *file_data = (const char *)file->address;
        uint64_t file_size = file->size;

        for (uint64_t byte_idx = 0; byte_idx < file_size; byte_idx++) {
            if (rangehigh >= MAX_LINES) break;

            char current_byte = file_data[byte_idx];

            if (current_byte == '\n' || current_byte == '\r') {
                if (current_byte == '\r' && (byte_idx + 1 < file_size) && file_data[byte_idx + 1] == '\n') {
                    byte_idx++; 
                }
                
                output[rangehigh][range] = '\0'; 
                range = 0;                       
                rangehigh++;                     
                continue;
            }

            output[rangehigh][range] = current_byte;
            range++;

            if (range >= (MAX_CHARS - 1)) {
                output[rangehigh][range] = '\0'; 
                range = 0;                       
                rangehigh++;                     
            }
        }

        if (range > 0 && rangehigh < MAX_LINES) {
            output[rangehigh][range] = '\0';
            rangehigh++;
        }

        return 0; 
    }

    return -2; 
}