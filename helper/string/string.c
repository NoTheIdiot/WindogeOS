#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define INT_MIN (-2147483647 - 1)
#define INT_MAX (2147483647)

int str_strcmp(const char* str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

void str_pad(char *dest, const char *src, int target_len, char pad_char) {
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


size_t str_strlen(const char *str) {
    if (!str) return 0;
    size_t counter = 0;
    while (str[counter] != '\0') {
        counter++;
    }
    return counter;
}

void str_itoa(int n, char* str) {
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

void str_u64toa(uint64_t n, char* str) {
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

int str_strncmp(const char *str1, const char *str2, size_t n) {
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

int str_startswith(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) {
            return 0;
        }
        str++;
        prefix++;
    }
    return 1;
}

int str_atoi(char* s) {
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

void str_strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

char* str_strncpy(char* dest, const char* src, size_t n) {
    if (n == 0) return dest;
    
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

char* str_strcat(char* dest, const char* src) {  
    char *ptr = dest;
    while (*ptr != '\0') {
        ptr++;
    }
    while ((*ptr++ = *src++));
    return dest;
}

void str_split_filename(const char *input, char *name, char *ext) {
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
