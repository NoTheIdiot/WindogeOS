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
int strcmp(const char *str1, const char *str2);
char* uint64_to_str(uint64_t value, char* buf);
void clean_input_string(char* str);
size_t strlen(const char* str);
void pad(char *dest, const char *src, int target_len, char pad_char);
void itoa(int n, char* str);
uint8_t to_u8(char *str);
void ltoa(uint64_t num, char* str);
void kutoa(uint64_t value, char *str);
int strncmp(const char *str1, const char *str2, size_t n);
int startswith(const char *str, const char *prefix);
int atoi(char* s);
void strcpy(char *dest, const char *src);
int strncpy(char* dest, const char* src, size_t n);
void u16_to_str(uint16_t value, char *out);
uint16_t to_u16(const char *s);
float atof(const char *str);
char* strcat(char* dest, const char* src);
void split_filename(const char *input, char *name, char *ext);
char u8tochar(uint8_t input);
uint8_t chartou8(char input);
// regular str stuff
int strcmp(const char *str1, const char *str2);
size_t strlen(const char* str);
void itoa(int n, char* str);
int strncmp(const char *str1, const char *str2, size_t n);
int startswith(const char *str, const char *prefix);
int atoi(char* s);
void strcpy(char *dest, const char *src);
float atof(const char *str);
char* strcat(char* dest, const char* src);
void split_filename(const char *input, char *name, char *ext);

#endif
