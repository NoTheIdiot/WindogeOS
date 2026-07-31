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

// regular str stuff
int str_strcmp(const char *str1, const char *str2);
size_t str_strlen(const char* str);
void str_itoa(int n, char* str);
int str_strncmp(const char *str1, const char *str2, size_t n);
int str_startswith(const char *str, const char *prefix);
int str_atoi(char* s);
void str_strcpy(char *dest, const char *src);
char* str_strncpy(char* dest, const char* src, size_t n);
float str_atof(const char *str);
char* str_strcat(char* dest, const char* src);
void str_split_filename(const char *input, char *name, char *ext);
char str_u8tochar(uint8_t input);
uint8_t str_chartou8(char input);
// regular str stuff
int str_strcmp(const char *str1, const char *str2);
size_t str_strlen(const char* str);
void str_itoa(int n, char* str);
int str_strncmp(const char *str1, const char *str2, size_t n);
int str_startswith(const char *str, const char *prefix);
int str_atoi(char* s);
void str_strcpy(char *dest, const char *src);
char* str_strncpy(char* dest, const char* src, size_t n);
float str_atof(const char *str);
char* str_strcat(char* dest, const char* src);
void str_split_filename(const char *input, char *name, char *ext);

#endif
