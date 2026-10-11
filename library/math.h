#ifndef MATH_H
#define MATH_H
#pragma once

typedef struct {
    double real;
    double imag;
} complex_t;

static inline double os_sqrt(double x)    { return __builtin_sqrt(x); }
static inline double os_atan2(double y, double x) { return __builtin_atan2(y, x); }
static inline double os_exp(double x)     { return __builtin_exp(x); }
static inline double os_sin(double x)     { return __builtin_sin(x); }
static inline double os_cos(double x)     { return __builtin_cos(x); }
static inline double os_sinh(double x)    { return __builtin_sinh(x); }
static inline double os_cosh(double x)    { return __builtin_cosh(x); }

static inline complex_t complex_add(complex_t a, complex_t b) {
    complex_t res = { .real = a.real + b.real, .imag = a.imag + b.imag };
    return res;
}

static inline complex_t complex_sub(complex_t a, complex_t b) {
    complex_t res = { .real = a.real - b.real, .imag = a.imag - b.imag };
    return res;
}

static inline complex_t complex_mul(complex_t a, complex_t b) {
    complex_t res = {
        .real = (a.real * b.real) - (a.imag * b.imag),
        .imag = (a.real * b.imag) + (a.imag * b.real)
    };
    return res;
}

static inline complex_t complex_div(complex_t a, complex_t b) {
    double denom = (b.real * b.real) + (b.imag * b.imag);
    complex_t res = {
        .real = ((a.real * b.real) + (a.imag * b.imag)) / denom,
        .imag = ((a.imag * b.real) - (a.real * b.imag)) / denom
    };
    return res;
}

static inline double complex_abs(complex_t z) {
    return os_sqrt((z.real * z.real) + (z.imag * z.imag));
}

static inline double complex_arg(complex_t z) {
    return os_atan2(z.imag, z.real);
}

static inline complex_t complex_exp(complex_t z) {
    double r = os_exp(z.real);
    complex_t res = {
        .real = r * os_cos(z.imag),
        .imag = r * os_sin(z.imag)
    };
    return res;
}

static inline complex_t complex_sin(complex_t z) {
    complex_t res = {
        .real = os_sin(z.real) * os_cosh(z.imag),
        .imag = os_cos(z.real) * os_sinh(z.imag)
    };
    return res;
}

static inline complex_t complex_cos(complex_t z) {
    complex_t res = {
        .real = os_cos(z.real) * os_cosh(z.imag),
        .imag = -os_sin(z.real) * os_sinh(z.imag)
    };
    return res;
}

static inline uint64_t math_power(uint64_t base, int exp) {
    if (exp < 0) {
        return 0;
    }
    uint64_t res = 1;
    uint64_t b = base;
    while (exp > 0) {
        if (exp & 1) {
            res *= b;
        }
        b *= b;
        exp >>= 1;
    }
    return res;
}

#endif