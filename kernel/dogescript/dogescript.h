#ifndef DOGESCRIPT_H
#define DOGESCRIPT_H

#include <stdint.h>
#include <stddef.h>
#include <bool.h>

// unmodifiable values before if you do everything dies

// max labels
#define MAX_LABELS 64

// 96 bytes
typedef struct {
    char variable_name[16];
    uint64_t value;
    char string_value[71];
    bool is_string;
} variable;

// 12 kilobytes
extern variable variable_stack[8];
extern size_t latest_variable_free;

// change a variable
int change_variable(char* name, uint64_t value, char* string_value, bool is_string);

// for some reason i need this
variable* get_variable(char* name);

// set a variable
int set_variable(char* name, uint64_t value, char* string_value, bool is_string);

typedef struct {
    char label_name[16];
    const char* script_ptr;
} script_label;

int add_label(char* name, const char* ptr);
const char* find_label(char* name);
void scan_labels(const char* script_text);
int dogescript_execute(const char* script_text);

#endif 
