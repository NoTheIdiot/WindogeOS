#ifndef MATH_H
#define MATH_H

#include <stdint.h>

uint64_t math_power(uint64_t base, int exponent);
uint64_t math_square_power(uint64_t base);
uint64_t math_root(uint64_t base, int root);
uint64_t math_square_root(uint64_t base);

uint64_t math_bytes_to_kilo(uint64_t bytes);
uint64_t math_bytes_to_mega(uint64_t bytes);
uint64_t math_kilo_to_mega(uint64_t kilo);
uint64_t math_mega_to_bytes(uint64_t mega);
uint64_t math_mega_to_kilo(uint64_t mega);
uint64_t math_kilo_to_bytes(uint64_t kilo);

#endif
