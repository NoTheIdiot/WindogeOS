#include <math.h>
#include <float.h>

float math_power(float base, float exp) {
    int exponent = (int)exp; 
    float result = 1.0f;
    int is_negative = 0;
    if (exponent < 0) {
        is_negative = 1;
        exponent = -exponent;
    }
    
    for (int i = 0; i < exponent; i++) {
        result = result * base;
    }

    return is_negative ? (1.0f / result) : result;
}

float math_square_power(float base) {
    return base * base;
}

float math_root(float base, float n) {
    if (base == 0.0f) return 0.0f;
    if (base == 1.0f) return 1.0f;
    if (n <= 0.0f)    return 0.0f; 

    float guess = base / n;
    if (guess < 1.0f) guess = 1.0f; 

    int power_idx = (int)n - 1;
    for (int i = 0; i < 10; i++) {
        float denominator = math_power(guess, power_idx);
        if (denominator == 0.0f) denominator = 0.000001f; 
        guess = (1.0f / n) * (((n - 1.0f) * guess) + (base / denominator));
    }

    return guess;
}

float math_square_root(float base) {
    float result = math_root(base, 2);
    return result;
}