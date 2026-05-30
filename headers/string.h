#ifndef STRING_H
#define STRING_H

#include <stddef.h>

int string_strcmp(const char *string1, const char *string2);
size_t string_strlen(const char *string);
void string_itoa(int n, char* string);
int string_strncmp(const char *string1, const char *string2, size_t n);
int string_startswith(const char *string, const char *prefix);
int string_atoi(char *s);
void string_strcpy(char *dest, const char *src);
char* string_strncpy(char* dest, const char* src, size_t n);

#endif
