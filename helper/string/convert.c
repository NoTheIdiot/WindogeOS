#include <string.h>
#include <stdint.h>
#include <stddef.h>

char str_u8tochar(uint8_t input) {
    char output = (uint8_t)input;
    return output;
}

uint8_t str_chartou8(char input) {
    uint8_t output = (uint8_t)input;
    return output;
}