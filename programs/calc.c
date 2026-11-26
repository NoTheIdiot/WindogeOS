#include "../dogec/dogeio.h"
#include "../dogec/stdio.h"
#include "../dogec/string.h"
#include "../dogec/math.h"

int apply_power(int base, int exp) {
    if (exp < 0) return 0;
    if (exp == 0) return 1;
    
    int is_neg = (base < 0 && (exp % 2 != 0));
    uint64_t abs_base = (base < 0) ? (uint64_t)(-base) : (uint64_t)base;
    int res = (int)math_power(abs_base, exp);
    return is_neg ? -res : res;
}

void print_int(int val) {
    if (val < 0) {
        printf("-");
        val = -val;
    } else if (val == 0) {
        printf("0\n");
        return;
    }

    char inline_buf[32];
    int idx = 0;
    while (val > 0) {
        inline_buf[idx++] = '0' + (val % 10);
        val /= 10;
    }

    for (int i = idx - 1; i >= 0; i--) {
        char single_char_str[2] = { inline_buf[i], '\0' };
        printf(single_char_str);
    }
    printf("\n");
}

int evaluate(char* string, int* result) {
    if (string == (void*)0) {
        return 0;
    }

    int total_result = 0; 
    int current_term = 1; 
    int current_base = 0; 
    int current_num = 0;
    
    char term_op = '+';   
    char pow_op = 0;      
    int has_num = 0;

    for (int i = 0; ; i++) {
        char c = string[i];
        
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            continue; 
        }

        if (c >= '0' && c <= '9') {
            current_num = (current_num * 10) + (c - '0'); 
            has_num = 1;
            continue;
        } 
         
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '\0') {
            if (has_num) {
                if (pow_op == '^') {
                    current_base = apply_power(current_base, current_num);
                    pow_op = 0;
                } else {
                    current_base = current_num;
                }

                if (c != '^') {
                    if (term_op == '+') {
                        current_term = current_base;
                    } else if (term_op == '-') {
                        current_term = -current_base;
                    } else if (term_op == '*') {
                        current_term *= current_base; 
                    } else if (term_op == '/') {
                        if (current_base == 0) {
                            printf("Error: Division by zero\n");
                            return 0;
                        }
                        current_term /= current_base; 
                    }

                    if (c == '+' || c == '-' || c == '\0') {
                        total_result += current_term;
                        current_term = 0;
                    }
                }

                current_num = 0; 
                has_num = 0;
            }

            if (c == '\0') {
                break;
            } else if (c == '^') {
                pow_op = '^';
            } else {
                term_op = c;
            }
        } else {
            printf("Error: Invalid character\n");
            return 0;
        }
    }

    *result = total_result;
    return 1;
}

void strip_newline(char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n' || str[i] == '\r') {
            str[i] = '\0';
            break;
        }
    }
}

void _start(int argc, char** argv) {
    char* expr = (void*)0;

    if (argv != (void*)0) {
        if (argc >= 2 && argv[1] != (void*)0 && argv[1][0] != '\0') {
            expr = argv[1];
        } else if (argc == 1 && argv[0] != (void*)0 && argv[0][0] != '\0' && strcmp(argv[0], "calc") != 0) {
            expr = argv[0];
        }
    }

    if (expr == (void*)0) {
        printf("Interactive Calculator (type 'exit' to quit)\n");
        char input[256];
        while (1) {
            printf("> ");
            if (fgets(input, sizeof(input), stdin) == (void*)0) {
                break;
            }
            strip_newline(input);
            if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
                break;
            }
            if (input[0] == '\0') {
                continue;
            }
            int res = 0;
            if (evaluate(input, &res)) {
                printf("= ");
                print_int(res);
            }
        }
        exit();
        return;
    } 
    
    if (strcmp(expr, "--help") == 0) {
        printf("Calculator\n");
        printf("Usage: calc [expression]\n");
        printf("Example: calc \"2 + 3 * 4\"\n");
        exit();
        return;
    }

    int res = 0;
    if (evaluate(expr, &res)) {
        printf("Result: ");
        print_int(res);
    }
    
    exit();
}
