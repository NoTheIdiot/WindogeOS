#include <system.h>
#include <math.h>

int util_calc(char* string) {
    int total_result = 0; 
    int current_term = 0; 
    int current_num = 0;
    char last_op = '+'; 
    int has_num = 0;

    for (int i = 0; string[i] != '\0'; i++) {
        char c = string[i];
        
        if (c == ' ') {
            continue; 
        }

        if (c >= '0' && c <= '9') {
            current_num = (current_num * 10) + (c - '0'); 
            has_num = 1;
        } 
          
        if (c == '+' || c == '-' || c == '*' || c == '/' || string[i+1] == '\0') {
            if (has_num) {
                
                if (last_op == '+') {
                    current_term = current_num;
                } else if (last_op == '-') {
                    current_term = -current_num;
                } else if (last_op == '*') {
                    current_term *= current_num; 
                } else if (last_op == '/') {
                    if (current_num != 0) {
                        current_term /= current_num; 
                    } else {
                        current_term = 0; 
                    }
                }
                
                if (c == '+' || c == '-' || string[i+1] == '\0') {
                    total_result += current_term;
                    current_term = 0;
                }

                current_num = 0; 
                has_num = 0;
            }
            last_op = c; 
        }
    }

    return total_result;
}
