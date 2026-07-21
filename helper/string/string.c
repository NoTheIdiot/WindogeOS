#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define INT_MIN (-2147483647 - 1)
#define INT_MAX (2147483647)

int string_strcmp(const char* string1, const char *string2) {
    while (*string1 && (*string1 == *string2)) {
        string1++;
        string2++;
    }
    return *(const unsigned char*)string1 - *(const unsigned char*)string2;
}

size_t string_strlen(const char *string) {
    if (!string) return 0; // Basic null safety check
    size_t counter = 0;
    while (string[counter] != '\0') {
        counter++;
    }
    return counter;
}

void string_itoa(int n, char* string) {
    int i = 0;
    
    if (n == 0) {
        string[i++] = '0';
        string[i] = '\0';
        return;
    }

    // Use a negative-safe loop to completely prevent INT_MIN overflow
    int is_negative = (n < 0);
    
    while (n != 0) {
        int rem = n % 10;
        string[i++] = (char)((is_negative ? -rem : rem) + '0');
        n = n / 10;
    }

    if (is_negative) {
        string[i++] = '-';
    }
    string[i] = '\0';

    // Reverse string
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
    int sign = 1;

    // Handle leading signs
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        // Simple overflow prevention check
        if (res > (INT_MAX - (*s - '0')) / 10) {
            return (sign == 1) ? INT_MAX : INT_MIN;
        }
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

void string_strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

char* string_strncpy(char* dest, const char* src, size_t n) {
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

char* string_strcat(char* dest, const char* src) {  
    char *ptr = dest;
    while (*ptr != '\0') {
        ptr++;
    }
    while ((*ptr++ = *src++));
    return dest;
}

void string_split_filename(const char *input, char *name, char *ext) {
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