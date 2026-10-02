#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int util_calc(char* string) {
    int result = 0;
    int current_num = 0;
    char last_op = '+'; 
    int has_num = 0;

    for (int i = 0; string[i] != '\0'; i++) {
        char c = string[i];
        
        if (c >= '0' && c <= '9') {
            current_num = (current_num * 10) + (c - '0'); 
            has_num = 1;
        } 
        
        if (c == '+' || c == '-' || c == '*' || c == '/' || string[i+1] == '\0') {
            if (has_num) {
                if (last_op == '+') {
                    result += current_num;
                } else if (last_op == '-') {
                    result -= current_num;
                } else if (last_op == '*') {
                    result *= current_num;
                } else if (last_op == '/') {
                    result /= current_num;
                }
                current_num = 0; 
                has_num = 0;
            }
            last_op = c; 
        }
    }

    return result;
}
