#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>

typedef struct {
    char name[32];
    int variable;
} variable_int;

typedef struct{
    char name[32];
    char variable[128];
} variable_str;

variable_int stack_int[16] = {0};
variable_str stack_str[16] = {0};
int last_stack_int = 0;
int last_stack_str = 0;


static int setvar_int(char* varname, int value) {
    if (last_stack_int >= 16) {
        return 0;
    } else {
        str_strncpy(stack_int[last_stack_int].name, varname, 30);
        stack_int[last_stack_int].name[31] = '\0'; 
        stack_int[last_stack_int].variable= value;
        last_stack_int++;
        return 1;
    }
}

static int setvar_str(char* variable, char* value) {
    if (last_stack_str >= 16) {
        return 0;
    } else {
        str_strncpy(stack_str[last_stack_str].name, variable, 30);
        stack_str[last_stack_str].name[31] = '\0';
        str_strncpy(stack_str[last_stack_str].variable, value, 126);
        stack_str[last_stack_str].variable[127] = '\0';
        last_stack_str++;
        return 1;
    }
}

int system_dogescript_execute(char* line) {
    if (line == NULL) return 0;

    if (str_startswith(line, "print ")) {
        char* print_text = line + 6;
        char str_buffer[64];

        if (print_text[0] == '$') {
            char* thing = line + 7;
            for (int i = 0; i < last_stack_int; i++) {
                if (str_strcmp(thing, stack_str[i].name) == 0) {
                    dogeio_text_println(stack_str[i].variable);
                    return 1;
                }
                if (str_strcmp(thing, stack_int[i].name) == 0) {
                    str_itoa(stack_int[i].variable, str_buffer);
                    dogeio_text_println(str_buffer);
                    return 1;
                }
            }
        }

        dogeio_text_println(print_text);
        return 1;
    } 
    
    else if (str_startswith(line, "var_int ")) {
        char* after_cmd = line + 8;
        char name[32];
        char value[64];
        int name_idx = 0;
        int val_idx = 0;
        int found_equals = 0;
        int len = (int)str_strlen(after_cmd);

        for (int i = 0; i < len; i++) {
            char c = after_cmd[i];
            if (c == '=') { found_equals = 1; continue; }
            if (c == ' ') { continue; }
            if (found_equals == 0) {
                if (name_idx < 31) name[name_idx++] = c;
            } else {
                if (val_idx < 63) value[val_idx++] = c;
            }
        }
        name[name_idx] = '\0';
        value[val_idx] = '\0';

        if (name_idx > 0 && val_idx > 0) {
            int int_val = str_atoi(value);
            return setvar_int(name, int_val);
        }
        return 0;
    }
    
    else if (str_startswith(line, "var_str ")) {
        char* after_cmd = line + 8;
        char name[32];
        char value[128];
        int name_idx = 0;
        int val_idx = 0;
        int found_equals = 0;
        int in_quotes = 0;
        int len = (int)str_strlen(after_cmd);

        for (int i = 0; i < len; i++) {
            char c = after_cmd[i];
            if (!found_equals) {
                if (c == '=') { found_equals = 1; continue; }
                if (c == ' ') { continue; }
                if (name_idx < 31) name[name_idx++] = c;
            } else {
                if (c == '"') { in_quotes = !in_quotes; continue; }
                if (!in_quotes && c == ' ') { continue; }
                if (val_idx < 127) value[val_idx++] = c;
            }
        }
        name[name_idx] = '\0';
        value[val_idx] = '\0';

        if (name_idx > 0 && val_idx > 0) {
            return setvar_str(name, value);
        }
        return 0;
    }
    
    return 0;
}


int system_dogescript(char* file) {

}