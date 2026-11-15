#ifndef USER_STRING_H
#define USER_STRING_H

#include <stdint.h>
#include <stddef.h>

#define INT_MIN (-2147483647 - 1)
#define INT_MAX (2147483647)

static inline int strcmp(const char* str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

static inline char* uint64_to_str(uint64_t value, char* buf) {
    char* p = buf + 18; 
    *p = '\0';

    char hex_digits[] = "0123456789ABCDEF";

    if (value == 0) {
        *(--p) = '0';
        *(--p) = 'x';
        return p;
    }

    while (value > 0) {
        *(--p) = hex_digits[value & 0xF];
        value >>= 4;
    }

    *(--p) = 'x';
    *(--p) = '0';

    return p;
}


// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// They must be implemented as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// you forgot that i made the compiling script :P

static inline void *memcpy(void *__restrict__ dest, const void *__restrict__ src, size_t n) {
    uint8_t *__restrict__ pdest = (uint8_t *)dest;
    const uint8_t *__restrict__ psrc = (uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

static inline void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

static inline void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t*)dest;
    const uint8_t *psrc = (uint8_t*)src;

    if ((uintptr_t)src > (uintptr_t)dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if ((uintptr_t)src < (uintptr_t)dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
}

static inline int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (uint8_t*)s1;
    const uint8_t *p2 = (uint8_t*)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

static inline void clean_input_string(char* str) {
    if (!str) return;
    int len = 0;
    while (str[len] != '\0') len++;
    while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n' || str[len - 1] == ' ' || str[len - 1] == '\t')) {
        str[--len] = '\0';
    }
}

static inline void pad(char *dest, const char *src, int target_len, char pad_char) {
    int i = 0;
    while (src[i] != '\0' && i < target_len) {
        dest[i] = src[i];
        i++;
    }

    while (i < target_len) {
        dest[i] = pad_char;
        i++;
    }

    dest[i] = '\0';
}

static inline size_t strlen(const char *str) {
    if (!str) return 0;
    size_t counter = 0;
    while (str[counter] != '\0') {
        counter++;
    }
    return counter;
}

static inline void u16_to_str(uint16_t value, char *out) {
    char buf[6];
    int i = 0;
    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (value > 0) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    for (int j = 0; j < i; j++) {
        out[j] = buf[i - j - 1];
    }
    out[i] = '\0';
}

static inline uint16_t to_u16(const char *s) {
    uint16_t result = 0;
    while (*s) {
        if (*s < '0' || *s > '9') break;
        result = result * 10 + (uint16_t)(*s - '0');
        s++;
    }
    return result;
}

static inline void itoa(int n, char* str) {
    int i = 0;
    
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    int is_negative = (n < 0);
    
    while (n != 0) {
        int rem = n % 10;
        str[i++] = (char)((is_negative ? -rem : rem) + '0');
        n = n / 10;
    }

    if (is_negative) {
        str[i++] = '-';
    }
    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static inline void ltoa(uint64_t num, char* str) {
    char temp[32];
    int i = 0;
    
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    while (num > 0) {
        temp[i++] = (num % 10) + '0';
        num /= 10;
    }
    
    int j = 0;
    while (i > 0) {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}

static inline void kutoa(uint64_t value, char *str) {
    int i = 0;

    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    while (value > 0) {
        str[i++] = (value % 10) + '0';
        value /= 10;
    }
    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static inline void u64toa(uint64_t n, char* str) {
    int i = 0;
    
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    while (n != 0) {
        uint64_t rem = n % 10;
        str[i++] = (char)(rem + '0');
        n = n / 10;
    }

    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static inline int strncmp(const char *str1, const char *str2, size_t n) {
    while (n > 0 && *str1 && (*str1 == *str2)) {
        str1++;
        str2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

static inline int startswith(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) {
            return 0;
        }
        str++;
        prefix++;
    }
    return 1;
}

static inline int atoi(const char* s) {
    int res = 0;
    int sign = 1;

    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        if (res > (INT_MAX - (*s - '0')) / 10) {
            return (sign == 1) ? INT_MAX : INT_MIN;
        }
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

static inline void strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

static inline uint8_t to_u8(const char *str) {
    if (!str) return 0;

    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }

    uint32_t res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (uint32_t)(*str - '0');
        if (res > 255) return 255;
        str++;
    }

    return (uint8_t)res;
}

static inline int strncpy(char *destination, const char *source, size_t capacity) {
    if (destination == NULL || source == NULL || capacity == 0) {
        return -1;
    }
    size_t length = strlen(source);
    if (length >= capacity) {
        return -1;
    }
    for (size_t i = 0; i <= length; i++) {
        destination[i] = source[i];
    }
    return 0;
}

static inline char* strcat(char* dest, const char* src) {  
    char *ptr = dest;
    while (*ptr != '\0') {
        ptr++;
    }
    while ((*ptr++ = *src++));
    return dest;
}

static inline void split_filename(const char *input, char *name, char *ext) {
    int dot_index = -1;
    int length = 0;

    while (input[length] != '\0') {
        if (input[length] == '.') {
            dot_index = length;
        }
        length++;
    }
    int i = 0;
    while (i < dot_index) {
        name[i] = input[i];
        i++;
    }
    name[i] = '\0'; 

    int j = 0;
    while (dot_index != -1 && (dot_index + 1 + j) < length) {
        ext[j] = input[dot_index + 1 + j];
        j++;
    }
    ext[j] = '\0'; 
}

#endif
