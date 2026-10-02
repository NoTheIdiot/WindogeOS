#include <stdint.h>
#include <math.h>

void fpu_init(void) {
    __asm__ volatile (
        "clts\n\t"
        "mov %cr0, %rax\n\t"
        "and $~0x4, %rax\n\t"
        "or $0x2, %rax\n\t"
        "mov %rax, %cr0\n\t"
        
        "mov %cr4, %rax\n\t"
        "or $(1 << 9 | 1 << 10), %rax\n\t"
        "mov %rax, %cr4\n\t"
        
        "fninit\n\t"
    );
}

uint64_t math_power(uint64_t base, int exponent) {
    if (exponent < 0) {
        return 0;
    }
    if (exponent == 0) {
        return 1;
    }

    uint64_t result = 1;
    for (int i = 0; i < exponent; i++) {
        result *= base;
    }

    return result;
}

uint64_t math_square_power(uint64_t base) {
    return base * base;
}

uint64_t math_root(uint64_t base, int root) {
    if (base == 0) {
        return 0;
    }
    if (base == 1 || root == 1) {
        return base;
    }
    if (root <= 0) {
        return 0;
    }

    uint64_t guess = base / (uint64_t)root;
    if (guess < 1) {
        guess = 1;
    }

    for (int i = 0; i < 25; i++) {
        uint64_t denominator = math_power(guess, root - 1);
        if (denominator == 0) {
            denominator = 1;
        }

        uint64_t next = (((uint64_t)(root - 1) * guess) + (base / denominator)) / (uint64_t)root;
        
        uint64_t diff = (next > guess) ? (next - guess) : (guess - next);
        if (diff == 0) {
            guess = next;
            break;
        }
        guess = next;
    }

    return guess;
}

uint64_t math_square_root(uint64_t base) {
    return math_root(base, 2);
}
