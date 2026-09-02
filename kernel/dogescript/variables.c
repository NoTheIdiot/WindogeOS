#include <system.h>
#include <core.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <dogeio.h>
#include <bool.h>
#include "dogescript.h"

// 12 kilobytes
variable variable_stack[128];
size_t latest_variable_free = 0;

int change_variable(char* name, uint64_t value, char* string_value, bool is_string) {
    if (name == NULL || (is_string && string_value == NULL)) {
        return 0;
    }

    for (size_t i = 0; i < latest_variable_free; i++) {
        if (str_strcmp(variable_stack[i].variable_name, name) == 0) {
            variable_stack[i].is_string = is_string;
            if (is_string) {
                str_strncpy(variable_stack[i].string_value, string_value, 70);
                variable_stack[i].string_value[70] = '\0';
                variable_stack[i].value = 0;
            } else {
                variable_stack[i].value = value;
                variable_stack[i].string_value[0] = '\0';
            }
            return 1;
        }
    }
    return 0;
}

variable* get_variable(char* name) {
    if (name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < latest_variable_free; i++) {
        if (str_strcmp(variable_stack[i].variable_name, name) == 0) {
            return &variable_stack[i];
        }
    }
    return NULL;
}

// set a variable
int set_variable(char* name, uint64_t value, char* string_value, bool is_string) {
    if (name == NULL || (is_string && string_value == NULL)) {
        return 0;
    }

    if (change_variable(name, value, string_value, is_string)) {
        return 1;
    }

    if (latest_variable_free >= 128) {
        return 0;
    }

    variable_stack[latest_variable_free].is_string = is_string;
    str_strncpy(variable_stack[latest_variable_free].variable_name, name, 15);
    variable_stack[latest_variable_free].variable_name[15] = '\0';

    if (is_string) {
        str_strncpy(variable_stack[latest_variable_free].string_value, string_value, 70);
        variable_stack[latest_variable_free].string_value[70] = '\0';
        variable_stack[latest_variable_free].value        = 0;
    } else {
        variable_stack[latest_variable_free].value = value;
        variable_stack[latest_variable_free].string_value[0] = '\0';
    }

    latest_variable_free++;
    return 1;
}
