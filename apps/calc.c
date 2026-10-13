#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/math.h"

void _start(int argc, char** argv) {
    if (argc < 1 || argv == (void*)0 || argv[0] == (void*)0) {
        print("Usage: calc <expression>\n");
        sys_exit(1);
    } 
    
    if (argv[0] != NULL && str_strcmp(argv[0], "--help") == 0) {
        print("Calculator\n");
        print("Usage: calc <expression>\n");
        print("Example: calc \"2 + 3 * 4\"\n");
        sys_exit(0);
    }

    char* string = argv[0];
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
          
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || string[i+1] == '\0') {
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
                } else if (last_op == '^') {
                    current_term = (int)math_power((uint64_t)(current_term), current_num);
                }
                
                if (c == '+' || c == '-' || string[i+1] == '\0') {
                    total_result += current_term;
                    current_term = 0;
                }

                current_num = 0; 
                has_num = 0;
            }
            if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
                last_op = c; 
            }
        }
    }

    print("Result: ");
    if (total_result < 0) {
        print("-");
        total_result = -total_result;
    } else if (total_result == 0) {
        print("0\n");
        sys_exit(0);
    }

    char inline_buf[32];
    int idx = 0;
    while (total_result > 0) {
        inline_buf[idx++] = '0' + (total_result % 10);
        total_result /= 10;
    }

    for (int i = idx - 1; i >= 0; i--) {
        char single_char_str[2] = { inline_buf[i], '\0' };
        print(single_char_str);
    }
    
    print("\n");
    sys_exit(0);
}
