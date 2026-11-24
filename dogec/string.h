#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

static inline char _doge_to_lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static inline void *memcpy(void *__restrict__ dst, const void *__restrict__ src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

static inline void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dst;
}

static inline void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}

static inline int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

static inline void *memchr(const void *s, int c, size_t n) {
    const uint8_t *p = (const uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == (uint8_t)c) return (void *)(p + i);
    }
    return NULL;
}

static inline size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static inline size_t strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len]) len++;
    return len;
}

static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)(uint8_t)*s1 - (int)(uint8_t)*s2;
}

static inline int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (int)(uint8_t)s1[i] - (int)(uint8_t)s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

static inline int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && (_doge_to_lower_ascii(*s1) == _doge_to_lower_ascii(*s2))) {
        s1++;
        s2++;
    }
    return (int)(uint8_t)_doge_to_lower_ascii(*s1) - (int)(uint8_t)_doge_to_lower_ascii(*s2);
}

static inline int strncasecmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = _doge_to_lower_ascii(s1[i]);
        char c2 = _doge_to_lower_ascii(s2[i]);
        if (c1 != c2) return (int)(uint8_t)c1 - (int)(uint8_t)c2;
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

static inline char *strcpy(char *__restrict__ dst, const char *__restrict__ src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

static inline char *strncpy(char *__restrict__ dst, const char *__restrict__ src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

static inline char *strcat(char *__restrict__ dst, const char *__restrict__ src) {
    char *ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return ret;
}

static inline char *strncat(char *__restrict__ dst, const char *__restrict__ src, size_t n) {
    char *ret = dst;
    while (*dst) dst++;
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dst[i] = src[i];
    dst[i] = '\0';
    return ret;
}

static inline char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

static inline char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char *)last;
}

static inline char *strstr(const char *haystack, const char *needle) {
    if (*needle == '\0') return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        if (*n == '\0') return (char *)haystack;
    }
    return NULL;
}

static inline size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    while (*s && strchr(accept, *s)) {
        count++;
        s++;
    }
    return count;
}

static inline size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    while (*s && !strchr(reject, *s)) {
        count++;
        s++;
    }
    return count;
}

static inline char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        if (strchr(accept, *s)) return (char *)s;
        s++;
    }
    return NULL;
}

static inline char *strtok(char *__restrict__ str, const char *__restrict__ delim) {
    static char *next = NULL;
    if (str != NULL) next = str;
    if (next == NULL) return NULL;

    next += strspn(next, delim);
    if (*next == '\0') {
        next = NULL;
        return NULL;
    }

    char *token = next;
    next += strcspn(next, delim);
    if (*next != '\0') {
        *next++ = '\0';
    } else {
        next = NULL;
    }
    return token;
}

#endif /* STRING_H */