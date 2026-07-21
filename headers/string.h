#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

#ifndef restrict
#define restrict __restrict
#endif

// memory stuff
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

// regular string stuff
int string_strcmp(const char *string1, const char *string2);
size_t string_strlen(const char* string);
void string_itoa(int n, char* string);
int string_strncmp(const char *string1, const char *string2, size_t n);
int string_startswith(const char *string, const char *prefix);
int string_atoi(char* s);
void string_strcpy(char *dest, const char *src);
char* string_strncpy(char* dest, const char* src, size_t n);
float string_atof(const char *str);
char* string_strcat(char* dest, const char* src);
void string_split_filename(const char *input, char *name, char *ext);

#endif
