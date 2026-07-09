#include <dogeio.h>
#include <drivers.h>

// use qemu for this dumbass
// no "basic_" because of it's going to use alot
void dogeio_log(const char* string) {
    serial_print(string);
    serial_print("\n");
}

// same thing but it prints both in the OS and in serial
void dogeio_duolog(const char* string) {
    dogeio_text_println(string);
    serial_println(string);
}