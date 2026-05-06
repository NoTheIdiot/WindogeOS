// include files
#include <stdint.h>
#include <stddef.h>

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
