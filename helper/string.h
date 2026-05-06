#ifndef STRING_H
#define STRING_H

int string_strcmp(const char *string1, const char *string2);
size_t string_strlen(const char *string);
void string_itoa(int n, char* string);
int string_strncmp(const char *string1, const char *string2, size_t n);
int string_startswith(const char *string, const char *prefix);

#endif