#include <dogeio.h>

// use qemu for this dumbass
// no "basic_" because of it's going to use alot
void dogeio_log(const char* string) {
    dogeio_text_serial_print(string);
    dogeio_text_serial_print("\n");
}

// same thing but it prints both in the OS and in serial
void dogeio_duolog(const char* string) {
    dogeio_text_println(string);
    dogeio_text_serial_println(string);
}