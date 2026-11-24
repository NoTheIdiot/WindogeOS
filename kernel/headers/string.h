#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

void  *memcpy(void *__restrict__ dst, const void *__restrict__ src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
int    memcmp(const void *s1, const void *s2, size_t n);
void  *memchr(const void *s, int c, size_t n);

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);
int    strcasecmp(const char *s1, const char *s2);
int    strncasecmp(const char *s1, const char *s2, size_t n);

char  *strcpy(char *__restrict__ dst, const char *__restrict__ src);
char  *strncpy(char *__restrict__ dst, const char *__restrict__ src, size_t n);
char  *strcat(char *__restrict__ dst, const char *__restrict__ src);
char  *strncat(char *__restrict__ dst, const char *__restrict__ src, size_t n);

char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char  *strpbrk(const char *s, const char *accept);
char  *strtok(char *__restrict__ str, const char *__restrict__ delim);

#endif
