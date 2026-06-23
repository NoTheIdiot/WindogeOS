#include <file.h>
#include <dogeio.h>

// a datatyppe that is... nothing
typedef void (*program_entry_t)(void);

void system_load_program(char* path, char* name, char* extension) {
    // assume that the program starts at 0x00200000
    uint8_t* program = (uint8_t*)0x00200000;
    int bytes_read = fat32_read_file("/", name, extension, program_buffer);
    if (bytes_read > 0) {
        program_entry_t start_program = (program_entry_t)program_buffer;
        start_program();
    } else {
        dogeio_println("Program not found, no fix for it inside the operating itself");
        dogeio_println("right now.");
    }

}