// include files
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <float.h>

// stringing compare
int string_strcmp(const char *string1, const char *string2) {
    while (*string1 && (*string1 == *string2)) {
        string1++;
        string2++;
    }
    return *(const unsigned char*)string1 - *(const unsigned char*)string2;
}

size_t string_strlen(const char *string) {
    size_t counter = 0;
    while (string[counter] != '\0') {
        counter++;
    }
    return counter;
}

void string_itoa(int n, char* string) {
    int i = 0;
    int is_negative = 0;

    if (n == 0) {
        string[i++] = '0';
        string[i] = '\0';
        return;
    }

    if (n < 0) {
        is_negative = 1;
        n = -n;
    }

    while (n != 0) {
        string[i++] = (n % 10) + '0';
        n = n / 10;
    }

    if (is_negative) {
        string[i++] = '-';
    }

    string[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = string[start];
        string[start] = string[end];
        string[end] = temp;
        start++;
        end--;
    }
}

int string_strncmp(const char *string1, const char *string2, size_t n) {
    while (n > 0 && *string1 && (*string1 == *string2)) {
        string1++;
        string2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(const unsigned char*)string1 - *(const unsigned char*)string2;
}

int string_startswith(const char *string, const char *prefix) {
    while (*prefix) {
        if (*string != *prefix) {
            return 0;
        }
        string++;
        prefix++;
    }
    return 1;
}

int string_atoi(char* s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}

float string_atof(const char *str) {
    float result = 0.0f;
    float fraction = 1.0f;
    int is_negative = 0;
    int point_seen = 0;

    if (*str == '-') {
        is_negative = 1;
        str++;
    }

    while (*str) {
        if (*str == '.') {
            point_seen = 1;
            str++;
            continue;
        }
        if (*str >= '0' && *str <= '9') {
            if (point_seen) {
                fraction *= 0.1f;
                result += (*str - '0') * fraction;
            } else {
                result = (result * 10.0f) + (*str - '0');
            }
        } else {
            break; 
        }
        str++;
    }
    return is_negative ? -result : result;
}

void string_strcpy(char *dest, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// did i just copy unsafe code
char* string_strncpy(char* dest, const char* src, size_t n) {
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

char* string_strcat(char* dest, const char* src) {  
    int i = 0;
    while (dest[i] != '\0') 
    {
        i++;
    }

    int j = 0;
    while (src[j] != '\0') 
    {
        dest[i] = src[j];
        i++;
        j++;
    }

    dest[i] = '\0';
    return dest;
}
