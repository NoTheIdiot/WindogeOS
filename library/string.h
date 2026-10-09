#ifndef USER_STRING_H
#define USER_STRING_H

#include "stddef.h"
#include "stdint.h"

#ifndef restrict
#define restrict __restrict
#endif

static inline void *memcpy(void *restrict dest, const void *restrict src,
                           size_t n) {
    uint8_t *restrict dest_bytes = dest;
    const uint8_t *restrict src_bytes = src;

    for (size_t i = 0; i < n; i++) {
        dest_bytes[i] = src_bytes[i];
    }
    return dest;
}

static inline void *memset(void *dest, int value, size_t n) {
    uint8_t *dest_bytes = dest;

    for (size_t i = 0; i < n; i++) {
        dest_bytes[i] = (uint8_t)value;
    }
    return dest;
}

static inline void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *dest_bytes = dest;
    const uint8_t *src_bytes = src;

    if ((uintptr_t)src < (uintptr_t)dest) {
        for (size_t i = n; i > 0; i--) {
            dest_bytes[i - 1] = src_bytes[i - 1];
        }
    } else if ((uintptr_t)src > (uintptr_t)dest) {
        for (size_t i = 0; i < n; i++) {
            dest_bytes[i] = src_bytes[i];
        }
    }
    return dest;
}

static inline int memcmp(const void *left, const void *right, size_t n) {
    const uint8_t *left_bytes = left;
    const uint8_t *right_bytes = right;

    for (size_t i = 0; i < n; i++) {
        if (left_bytes[i] != right_bytes[i]) {
            return left_bytes[i] < right_bytes[i] ? -1 : 1;
        }
    }
    return 0;
}

static inline int str_strcmp(const char *str1, const char *str2) {
    while (*str1 != '\0' && *str1 == *str2) {
        str1++;
        str2++;
    }
    return (int)(uint8_t)*str1 - (int)(uint8_t)*str2;
}

static inline int str_strncmp(const char *str1, const char *str2, size_t n) {
    while (n > 0 && *str1 != '\0' && *str1 == *str2) {
        str1++;
        str2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (int)(uint8_t)*str1 - (int)(uint8_t)*str2;
}

static inline size_t str_strlen(const char *str) {
    size_t length = 0;
    if (str == NULL) {
        return 0;
    }
    while (str[length] != '\0') {
        length++;
    }
    return length;
}

static inline int str_startswith(const char *str, const char *prefix) {
    while (*prefix != '\0') {
        if (*str != *prefix) {
            return 0;
        }
        str++;
        prefix++;
    }
    return 1;
}

static inline void str_strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++) != '\0') {
    }
}

static inline char *str_strncpy(char *dest, const char *src, size_t n) {
    if (n == 0) {
        return dest;
    }

    size_t i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    while (i < n) {
        dest[i] = '\0';
        i++;
    }
    return dest;
}

static inline char *str_strcat(char *dest, const char *src) {
    char *result = dest;
    while (*dest != '\0') {
        dest++;
    }
    while ((*dest++ = *src++) != '\0') {
    }
    return result;
}

static inline void str_pad(char *dest, const char *src, int target_len,
                           char pad_char) {
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

static inline void clean_input_string(char *str) {
    if (str == NULL) {
        return;
    }

    size_t length = str_strlen(str);
    while (length > 0 &&
           (str[length - 1] == '\r' || str[length - 1] == '\n' ||
            str[length - 1] == ' ' || str[length - 1] == '\t')) {
        str[--length] = '\0';
    }
}

static inline void str_itoa(int value, char *str) {
    size_t i = 0;
    int negative = value < 0;
    uint32_t magnitude = negative ? 0u - (uint32_t)value : (uint32_t)value;

    do {
        str[i++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0);

    if (negative) {
        str[i++] = '-';
    }
    str[i] = '\0';

    for (size_t start = 0, end = i - 1; start < end; start++, end--) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
    }
}

static inline void str_u64toa(uint64_t value, char *str) {
    size_t i = 0;
    do {
        str[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0);
    str[i] = '\0';

    for (size_t start = 0, end = i - 1; start < end; start++, end--) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
    }
}

static inline char *uint64_to_str(uint64_t value, char *buf) {
    static const char hex_digits[] = "0123456789ABCDEF";
    char *cursor = buf + 18;
    *cursor = '\0';

    do {
        *--cursor = hex_digits[value & 0xFu];
        value >>= 4;
    } while (value != 0);

    *--cursor = 'x';
    *--cursor = '0';
    return cursor;
}

static inline int str_atoi(char *str) {
    int negative = 0;
    uint32_t value = 0;
    uint32_t limit;

    if (*str == '-') {
        negative = 1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    limit = negative ? 2147483648u : 2147483647u;
    while (*str >= '0' && *str <= '9') {
        uint32_t digit = (uint32_t)(*str - '0');
        if (value > (limit - digit) / 10u) {
            return negative ? (-2147483647 - 1) : 2147483647;
        }
        value = value * 10u + digit;
        str++;
    }

    if (negative) {
        return value == 2147483648u ? (-2147483647 - 1) : -(int)value;
    }
    return (int)value;
}

static inline uint8_t str_to_u8(char *str) {
    if (str == NULL) {
        return 0;
    }

    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }

    uint32_t value = 0;
    while (*str >= '0' && *str <= '9') {
        uint32_t digit = (uint32_t)(*str - '0');
        if (value > (255u - digit) / 10u) {
            return 255;
        }
        value = value * 10u + digit;
        str++;
    }
    return (uint8_t)value;
}

static inline void u16_to_str(uint16_t value, char *out) {
    char digits[5];
    size_t length = 0;

    do {
        digits[length++] = (char)('0' + (value % 10u));
        value = (uint16_t)(value / 10u);
    } while (value != 0);

    for (size_t i = 0; i < length; i++) {
        out[i] = digits[length - i - 1];
    }
    out[length] = '\0';
}

static inline uint16_t str_to_u16(const char *str) {
    uint32_t value = 0;
    while (*str >= '0' && *str <= '9') {
        uint32_t digit = (uint32_t)(*str - '0');
        if (value > (65535u - digit) / 10u) {
            return 65535;
        }
        value = value * 10u + digit;
        str++;
    }
    return (uint16_t)value;
}

static inline char str_u8tochar(uint8_t value) {
    return (char)value;
}

static inline uint8_t str_chartou8(char value) {
    return (uint8_t)value;
}

static inline void str_split_filename(const char *input, char *name,
                                      char *ext) {
    size_t length = 0;
    size_t dot_index = SIZE_MAX;

    while (input[length] != '\0') {
        if (input[length] == '.') {
            dot_index = length;
        }
        length++;
    }

    size_t name_length = dot_index == SIZE_MAX ? length : dot_index;
    for (size_t i = 0; i < name_length; i++) {
        name[i] = input[i];
    }
    name[name_length] = '\0';

    size_t ext_length = dot_index == SIZE_MAX ? 0 : length - dot_index - 1;
    for (size_t i = 0; i < ext_length; i++) {
        ext[i] = input[dot_index + 1 + i];
    }
    ext[ext_length] = '\0';
}

#endif