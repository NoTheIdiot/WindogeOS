#include <math.h>
#include <float.h>

float math_power(float base, float exp) {
    int exponent = (int)exp;
    if ((float)exponent != exp) {
        return 0.0f;
    }

    float result = 1.0f;
    int is_negative = 0;
    if (exponent < 0) {
        is_negative = 1;
        exponent = -exponent;
    }

    for (int i = 0; i < exponent; i++) {
        result *= base;
    }

    return is_negative ? (1.0f / result) : result;
}

float math_square_power(float base) {
    return base * base;
}

float math_root(float base, float n) {
    if (base <= 0.0f) {
        return base == 0.0f ? 0.0f : 0.0f;
    }
    if (base == 1.0f) {
        return 1.0f;
    }

    int root = (int)n;
    if ((float)root != n || root <= 0) {
        return 0.0f;
    }

    float guess = base / (float)root;
    if (guess < 1.0f) {
        guess = 1.0f;
    }

    for (int i = 0; i < 25; i++) {
        float denominator = math_power(guess, (float)(root - 1));
        if (denominator == 0.0f) {
            denominator = 0.000001f;
        }

        float next = (((float)(root - 1) * guess) + (base / denominator)) / (float)root;
        if ((next > guess ? next - guess : guess - next) < 0.000001f) {
            guess = next;
            break;
        }
        guess = next;
    }

    return guess;
}

float math_square_root(float base) {
    return math_root(base, 2.0f);
}