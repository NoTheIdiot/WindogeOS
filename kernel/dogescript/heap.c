#include <dogeio.h>
#include <stdint.h>
#include <system.h>
#include <stddef.h>
#include "dogescript.h"

int peek(uint64_t address, uint64_t* buffer) {
    if (address < MEM_START || address > MEM_END) {
        return 0;
    } 
    
    if (address % 8 != 0) {
        return 0;
    }

    volatile uint64_t* target_pointer = (volatile uint64_t*)address;
    *buffer = *target_pointer; 

    return 1;
}

int poke(uint64_t address, uint64_t value) {
    if (address < MEM_START || address > MEM_END) {
        return 0;
    }

    if (address % 8 != 0) {
        return 0;
    }

    volatile uint64_t* target_pointer = (volatile uint64_t*)address;
    *target_pointer = value;

    return 1;
}
