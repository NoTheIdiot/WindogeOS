#ifndef MATH_H
#define MATH_H
#pragma once

typedef struct {
    double real;
    double imag;
} complex_t;

static inline double math_sqrt(double x) { return __builtin_sqrt(x); }
static inline double math_cbrt(double x) { return __builtin_cbrt(x); }
static inline double math_atan2(double y, double x) { return __builtin_atan2(y, x); }
static inline double math_exp(double x) { return __builtin_exp(x); }
static inline double math_log(double x) { return __builtin_log(x); }
static inline double math_log10(double x) { return __builtin_log10(x); }
static inline double math_log2(double x) { return __builtin_log2(x); }
static inline double math_pow(double x, double y) { return __builtin_pow(x, y); }
static inline double math_sin(double x) { return __builtin_sin(x); }
static inline double math_cos(double x) { return __builtin_cos(x); }
static inline double math_tan(double x) { return __builtin_tan(x); }
static inline double math_asin(double x) { return __builtin_asin(x); }
static inline double math_acos(double x) { return __builtin_acos(x); }
static inline double math_atan(double x) { return __builtin_atan(x); }
static inline double math_sinh(double x) { return __builtin_sinh(x); }
static inline double math_cosh(double x) { return __builtin_cosh(x); }
static inline double math_tanh(double x) { return __builtin_tanh(x); }
static inline double math_asinh(double x) { return __builtin_asinh(x); }
static inline double math_acosh(double x) { return __builtin_acosh(x); }
static inline double math_atanh(double x) { return __builtin_atanh(x); }
static inline double math_fabs(double x) { return __builtin_fabs(x); }
static inline double math_ceil(double x) { return __builtin_ceil(x); }
static inline double math_floor(double x) { return __builtin_floor(x); }
static inline double math_round(double x) { return __builtin_round(x); }
static inline double math_fmin(double x, double y) { return x < y ? x : y; }
static inline double math_fmax(double x, double y) { return x > y ? x : y; }
static inline double math_fmod(double x, double y) { return __builtin_fmod(x, y); }

static inline complex_t math_complex_add(complex_t a, complex_t b) {
    complex_t res = { .real = a.real + b.real, .imag = a.imag + b.imag };
    return res;
}

static inline complex_t math_complex_sub(complex_t a, complex_t b) {
    complex_t res = { .real = a.real - b.real, .imag = a.imag - b.imag };
    return res;
}

static inline complex_t math_complex_mul(complex_t a, complex_t b) {
    complex_t res = {
        .real = (a.real * b.real) - (a.imag * b.imag),
        .imag = (a.real * b.imag) + (a.imag * b.real)
    };
    return res;
}

static inline complex_t math_complex_div(complex_t a, complex_t b) {
    double denom = (b.real * b.real) + (b.imag * b.imag);
    complex_t res = {
        .real = ((a.real * b.real) + (a.imag * b.imag)) / denom,
        .imag = ((a.imag * b.real) - (a.real * b.imag)) / denom
    };
    return res;
}

static inline double math_complex_abs(complex_t z) {
    return math_sqrt((z.real * z.real) + (z.imag * z.imag));
}

static inline double math_complex_arg(complex_t z) {
    return math_atan2(z.imag, z.real);
}

static inline complex_t math_complex_exp(complex_t z) {
    double r = math_exp(z.real);
    complex_t res = {
        .real = r * math_cos(z.imag),
        .imag = r * math_sin(z.imag)
    };
    return res;
}

static inline complex_t math_complex_log(complex_t z) {
    complex_t res = {
        .real = math_log(math_complex_abs(z)),
        .imag = math_complex_arg(z)
    };
    return res;
}

static inline complex_t math_complex_sin(complex_t z) {
    complex_t res = {
        .real = math_sin(z.real) * math_cosh(z.imag),
        .imag = math_cos(z.real) * math_sinh(z.imag)
    };
    return res;
}

static inline complex_t math_complex_cos(complex_t z) {
    complex_t res = {
        .real = math_cos(z.real) * math_cosh(z.imag),
        .imag = -math_sin(z.real) * math_sinh(z.imag)
    };
    return res;
}

static inline complex_t math_complex_sqrt(complex_t z) {
    double r = math_complex_abs(z);
    double theta = math_complex_arg(z);
    double sr = math_sqrt(r);
    complex_t res = {
        .real = sr * math_cos(theta / 2.0),
        .imag = sr * math_sin(theta / 2.0)
    };
    return res;
}

static inline complex_t math_complex_pow(complex_t z, complex_t p) {
    complex_t l = math_complex_log(z);
    complex_t m = math_complex_mul(l, p);
    return math_complex_exp(m);
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