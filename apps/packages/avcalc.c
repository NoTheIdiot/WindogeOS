#include "../library/stdint.h"
#include "../library/stddef.h"
#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/string.h"
#include "../library/math.h"

static void int_to_str(int64_t val, char *buf, int max_len) {
    if (val == 0) {
        if (max_len > 1) {
            buf[0] = '0';
            buf[1] = '\0';
        }
        return;
    }
    int is_neg = 0;
    if (val < 0) {
        is_neg = 1;
        val = -val;
    }
    char temp[32];
    int idx = 0;
    int64_t tmp = val;
    while (tmp > 0) {
        temp[idx++] = (char)('0' + (tmp % 10));
        tmp /= 10;
    }
    int b_idx = 0;
    if (is_neg && b_idx < max_len - 1) {
        buf[b_idx++] = '-';
    }
    while (idx > 0 && b_idx < max_len - 1) {
        buf[b_idx++] = temp[--idx];
    }
    buf[b_idx] = '\0';
}

static void fixed_to_str(int64_t int_part, int64_t frac_part, char *buf, int max_len) {
    (void)max_len;
    char int_buf[32];
    int_to_str(int_part, int_buf, 32);
    str_strcpy(buf, int_buf);
    str_strcat(buf, ".");
    char frac_buf[32];
    int_to_str(frac_part, frac_buf, 32);
    int f_len = (int)str_strlen(frac_buf);
    for (int i = 0; i < 6 - f_len; i++) {
        str_strcat(buf, "0");
    }
    str_strcat(buf, frac_buf);
}

typedef struct {
    int64_t int_part;
    int64_t frac_part;
} fixed_t;

static fixed_t fixed_add(fixed_t a, fixed_t b) {
    fixed_t res;
    res.frac_part = a.frac_part + b.frac_part;
    res.int_part = a.int_part + b.int_part;
    if (res.frac_part >= 1000000) {
        res.frac_part -= 1000000;
        res.int_part += 1;
    } else if (res.frac_part <= -1000000) {
        res.frac_part += 1000000;
        res.int_part -= 1;
    }
    return res;
}

static fixed_t fixed_sub(fixed_t a, fixed_t b) {
    fixed_t res;
    res.frac_part = a.frac_part - b.frac_part;
    res.int_part = a.int_part - b.int_part;
    if (res.frac_part <= -1000000) {
        res.frac_part += 1000000;
        res.int_part -= 1;
    } else if (res.frac_part >= 1000000) {
        res.frac_part -= 1000000;
        res.int_part += 1;
    }
    return res;
}

static int64_t fixed_to_long(fixed_t a) {
    return a.int_part;
}

static fixed_t long_to_fixed(int64_t v) {
    fixed_t res = { .int_part = v, .frac_part = 0 };
    return res;
}

static fixed_t fixed_mul(fixed_t a, fixed_t b) {
    int64_t a_total = a.int_part * 1000000 + (a.int_part < 0 ? -a.frac_part : a.frac_part);
    int64_t b_total = b.int_part * 1000000 + (b.int_part < 0 ? -b.frac_part : b.frac_part);
    int64_t prod = (a_total * b_total) / 1000000;
    fixed_t res = { .int_part = prod / 1000000, .frac_part = prod % 1000000 };
    if (res.frac_part < 0) res.frac_part = -res.frac_part;
    return res;
}

static fixed_t fixed_div(fixed_t a, fixed_t b) {
    int64_t a_total = a.int_part * 1000000 + (a.int_part < 0 ? -a.frac_part : a.frac_part);
    int64_t b_total = b.int_part * 1000000 + (b.int_part < 0 ? -b.frac_part : b.frac_part);
    if (b_total == 0) {
        fixed_t zero = {0, 0};
        return zero;
    }
    int64_t quot = (a_total * 1000000) / b_total;
    fixed_t res = { .int_part = quot / 1000000, .frac_part = quot % 1000000 };
    if (res.frac_part < 0) res.frac_part = -res.frac_part;
    return res;
}

static fixed_t fixed_sin(fixed_t x) {
    int64_t x_total = x.int_part * 1000000 + x.frac_part;
    while (x_total > 3141592) x_total -= 6283185;
    while (x_total < -3141592) x_total += 6283185;
    int64_t x2 = (x_total * x_total) / 1000000;
    int64_t x3 = (x2 * x_total) / 1000000;
    int64_t x5 = (x3 * x2) / 1000000;
    int64_t x7 = (x5 * x2) / 1000000;
    int64_t term1 = x_total;
    int64_t term2 = x3 / 6;
    int64_t term3 = x5 / 120;
    int64_t term4 = x7 / 5040;
    int64_t res_total = term1 - term2 + term3 - term4;
    fixed_t res = { .int_part = res_total / 1000000, .frac_part = res_total % 1000000 };
    if (res.frac_part < 0) res.frac_part = -res.frac_part;
    return res;
}

static fixed_t fixed_cos(fixed_t x) {
    int64_t x_total = x.int_part * 1000000 + x.frac_part;
    while (x_total > 3141592) x_total -= 6283185;
    while (x_total < -3141592) x_total += 6283185;
    int64_t x2 = (x_total * x_total) / 1000000;
    int64_t x4 = (x2 * x2) / 1000000;
    int64_t x6 = (x4 * x2) / 1000000;
    int64_t term1 = 1000000;
    int64_t term2 = x2 / 2;
    int64_t term3 = x4 / 24;
    int64_t term4 = x6 / 720;
    int64_t res_total = term1 - term2 + term3 - term4;
    fixed_t res = { .int_part = res_total / 1000000, .frac_part = res_total % 1000000 };
    if (res.frac_part < 0) res.frac_part = -res.frac_part;
    return res;
}

static fixed_t fixed_tan(fixed_t x) {
    fixed_t s = fixed_sin(x);
    fixed_t c = fixed_cos(x);
    return fixed_div(s, c);
}

static fixed_t fixed_sqrt(fixed_t x) {
    if (x.int_part < 0) {
        fixed_t zero = {0, 0};
        return zero;
    }
    int64_t val = x.int_part * 1000000 + x.frac_part;
    int64_t guess = val / 2;
    if (guess == 0) guess = val;
    for (int i = 0; i < 15; i++) {
        if (guess == 0) break;
        int64_t next = (guess + val / guess) / 2;
        guess = next;
    }
    fixed_t res = { .int_part = guess / 1000000, .frac_part = guess % 1000000 };
    return res;
}

static fixed_t fixed_log(fixed_t x) {
    int64_t val = x.int_part * 1000000 + x.frac_part;
    if (val <= 0) {
        fixed_t zero = {0, 0};
        return zero;
    }
    int64_t num = val - 1000000;
    int64_t den = val + 1000000;
    int64_t term = (num * 1000000) / den;
    int64_t term_sq = (term * term) / 1000000;
    int64_t sum = term;
    int64_t cur = term;
    for (int n = 3; n < 15; n += 2) {
        cur = (cur * term_sq) / 1000000;
        sum += cur / n;
    }
    int64_t res_total = 2 * sum;
    fixed_t res = { .int_part = res_total / 1000000, .frac_part = res_total % 1000000 };
    if (res.frac_part < 0) res.frac_part = -res.frac_part;
    return res;
}

static fixed_t fixed_pow(fixed_t base, fixed_t exp) {
    int64_t e = fixed_to_long(exp);
    fixed_t res = long_to_fixed(1);
    for (int i = 0; i < e; i++) {
        res = fixed_mul(res, base);
    }
    return res;
}

static fixed_t fixed_factorial(fixed_t x) {
    int64_t n = fixed_to_long(x);
    if (n < 0) {
        fixed_t zero = {0, 0};
        return zero;
    }
    int64_t acc = 1;
    for (int64_t i = 2; i <= n && i <= 15; i++) {
        acc *= i;
    }
    return long_to_fixed(acc);
}

typedef struct {
    const char *p;
    fixed_t memory;
    int error;
} Parser;

static fixed_t parse_expr(Parser *ps);

static void skip_spaces(Parser *ps) {
    while (ps && ps->p && *ps->p == ' ') {
        ps->p++;
    }
}

static fixed_t parse_factor(Parser *ps) {
    if (!ps || !ps->p) {
        fixed_t z = {0, 0};
        return z;
    }
    skip_spaces(ps);
    if (!ps->p || *ps->p == '\0') {
        fixed_t z = {0, 0};
        return z;
    }
    if (*ps->p == '(') {
        ps->p++;
        fixed_t val = parse_expr(ps);
        skip_spaces(ps);
        if (ps->p && *ps->p == ')') {
            ps->p++;
        } else {
            ps->error = 1;
        }
        return val;
    }
    if (*ps->p == '-') {
        ps->p++;
        fixed_t f = parse_factor(ps);
        f.int_part = -f.int_part;
        f.frac_part = -f.frac_part;
        return f;
    }
    if (*ps->p == '+') {
        ps->p++;
        return parse_factor(ps);
    }
    if (str_startswith(ps->p, "sin")) { ps->p += 3; return fixed_sin(parse_factor(ps)); }
    if (str_startswith(ps->p, "cos")) { ps->p += 3; return fixed_cos(parse_factor(ps)); }
    if (str_startswith(ps->p, "tan")) { ps->p += 3; return fixed_tan(parse_factor(ps)); }
    if (str_startswith(ps->p, "sqrt")) { ps->p += 4; return fixed_sqrt(parse_factor(ps)); }
    if (str_startswith(ps->p, "log")) { ps->p += 3; return fixed_div(fixed_log(parse_factor(ps)), long_to_fixed(2)); }
    if (str_startswith(ps->p, "ln")) { ps->p += 2; return fixed_log(parse_factor(ps)); }
    if (str_startswith(ps->p, "abs")) { 
        ps->p += 3; 
        fixed_t f = parse_factor(ps); 
        if (f.int_part < 0) f.int_part = -f.int_part; 
        if (f.frac_part < 0) f.frac_part = -f.frac_part; 
        return f; 
    }
    if (str_startswith(ps->p, "pi")) { ps->p += 2; fixed_t pi = {3, 141592}; return pi; }
    if (str_startswith(ps->p, "e")) { ps->p++; fixed_t e = {2, 718281}; return e; }
    if (str_startswith(ps->p, "ans")) { ps->p += 3; return ps->memory; }

    int64_t int_val = 0;
    int64_t frac_val = 0;
    int has_num = 0;
    int decimal = 0;
    int64_t div_factor = 1;

    while (ps->p && *ps->p != '\0' && ((*ps->p >= '0' && *ps->p <= '9') || *ps->p == '.')) {
        has_num = 1;
        if (*ps->p == '.') {
            decimal = 1;
        } else {
            if (!decimal) {
                int_val = int_val * 10 + (*ps->p - '0');
            } else {
                div_factor *= 10;
                frac_val = frac_val * 10 + (*ps->p - '0');
            }
        }
        ps->p++;
    }

    if (!has_num) {
        ps->error = 1;
        fixed_t zero = {0, 0};
        return zero;
    }

    if (decimal && div_factor > 1) {
        while (div_factor < 1000000) {
            frac_val *= 10;
            div_factor *= 10;
        }
        while (div_factor > 1000000) {
            frac_val /= 10;
            div_factor /= 10;
        }
    }

    fixed_t val = { .int_part = int_val, .frac_part = frac_val };

    skip_spaces(ps);
    if (ps->p && *ps->p == '!') {
        ps->p++;
        val = fixed_factorial(val);
    }
    return val;
}

static fixed_t parse_power(Parser *ps) {
    fixed_t val = parse_factor(ps);
    while (ps && ps->p && *ps->p != '\0') {
        skip_spaces(ps);
        if (ps->p && *ps->p == '^') {
            ps->p++;
            fixed_t exp = parse_factor(ps);
            val = fixed_pow(val, exp);
        } else {
            break;
        }
    }
    return val;
}

static fixed_t parse_term(Parser *ps) {
    fixed_t val = parse_power(ps);
    while (ps && ps->p && *ps->p != '\0') {
        skip_spaces(ps);
        if (ps->p && *ps->p == '*') {
            ps->p++;
            val = fixed_mul(val, parse_power(ps));
        } else if (ps->p && *ps->p == '/') {
            ps->p++;
            fixed_t divisor = parse_power(ps);
            if (divisor.int_part == 0 && divisor.frac_part == 0) {
                ps->error = 2;
                fixed_t zero = {0, 0};
                return zero;
            }
            val = fixed_div(val, divisor);
        } else {
            break;
        }
    }
    return val;
}

static fixed_t parse_expr(Parser *ps) {
    fixed_t val = parse_term(ps);
    while (ps && ps->p && *ps->p != '\0') {
        skip_spaces(ps);
        if (ps->p && *ps->p == '+') {
            ps->p++;
            val = fixed_add(val, parse_term(ps));
        } else if (ps->p && *ps->p == '-') {
            ps->p++;
            val = fixed_sub(val, parse_term(ps));
        } else {
            break;
        }
    }
    return val;
}

static fixed_t evaluate(const char *expr, fixed_t last_ans, int *error_code) {
    Parser ps = { .p = expr, .memory = last_ans, .error = 0 };
    fixed_t res = parse_expr(&ps);
    *error_code = ps.error;
    return res;
}

#define MAX_HISTORY 10
#define INPUT_BUF_LEN 128

typedef struct {
    char query[INPUT_BUF_LEN];
    char result[32];
} HistoryEntry;

void _start(void) {
    clear();
    background_color(COLOR_BACKGROUND);
    text_color(COLOR_WHITE);
    clear();

    HistoryEntry history[MAX_HISTORY];
    int history_count = 0;
    char input_buf[INPUT_BUF_LEN];
    int input_len = 0;
    input_buf[0] = '\0';

    fixed_t last_answer = {0, 0};
    fixed_t memory_reg = {0, 0};
    int show_help = 0;

    char latest_query[INPUT_BUF_LEN] = "";
    char latest_result[32] = "0.000000";

    int needs_redraw = 1;

    while (1) {
        if (needs_redraw) {
            clear();
            print_at("=== AvCalc (Bad Advanced Calculator) ===", 2, 1, COLOR_BRIGHT_CYAN);
            print_at("---------------------------------------------------------", 2, 2, COLOR_DARK_GRAY);

            if (show_help) {
                print_at("HELP & COMMAND REFERENCE:", 4, 4, COLOR_YELLOW);
                print_at(" - Operators: + , - , * , / , ^ (power) , ! (factorial)", 4, 6, COLOR_WHITE);
                print_at(" - Functions: sin(x), cos(x), tan(x), sqrt(x), log(x), ln(x), abs(x)", 4, 8, COLOR_WHITE);
                print_at(" - Constants: pi, e, ans (last result)", 4, 10, COLOR_WHITE);
                print_at(" - Special Commands in input:", 4, 12, COLOR_YELLOW);
                print_at("   * 'clear' or 'cls' to reset history", 6, 13, COLOR_WHITE);
                print_at("   * 'mr' / 'mc' for Memory Register", 6, 14, COLOR_WHITE);
                print_at("CONTROLS: [CTRL+O] Toggle Help | [CTRL+N] Clear | [ENTER] Calculate", 2, 21, COLOR_BRIGHT_GREEN);
            } else {
                print_at("LATEST CALCULATION:", 4, 3, COLOR_BRIGHT_GREEN);
                char latest_box[200];
                str_strcpy(latest_box, "> ");
                str_strcat(latest_box, latest_query[0] ? latest_query : "(none)");
                str_strcat(latest_box, " = ");
                str_strcat(latest_box, latest_result);
                print_at(latest_box, 4, 4, COLOR_BRIGHT_WHITE);
                print_at("---------------------------------------------------------", 2, 5, COLOR_DARK_GRAY);

                print_at("HISTORY:", 4, 6, COLOR_BRIGHT_YELLOW);
                int start_y = 7;
                for (int i = 0; i < history_count && i < MAX_HISTORY; i++) {
                    char line_buf[200];
                    str_strcpy(line_buf, "  > ");
                    str_strcat(line_buf, history[i].query);
                    str_strcat(line_buf, " = ");
                    str_strcat(line_buf, history[i].result);
                    print_at(line_buf, 4, (uint32_t)(start_y + i), COLOR_GRAY);
                }
                if (history_count == 0) {
                    print_at("  (No previous history)", 4, 7, COLOR_DARK_WHITE);
                }

                char mem_str[64];
                str_strcpy(mem_str, "Memory [M]: ");
                char mem_val_str[32];
                fixed_to_str(memory_reg.int_part, memory_reg.frac_part, mem_val_str, 32);
                str_strcat(mem_str, mem_val_str);
                print_at(mem_str, 4, 18, COLOR_CYAN);
                print_at("CONTROLS: [CTRL+O] Help | [CTRL+N] Clear History | [ENTER] Calculate", 2, 21, COLOR_BRIGHT_GREEN);
            }

            print_at("---------------------------------------------------------", 2, 19, COLOR_DARK_GRAY);
            print_at("Expr > ", 2, 20, COLOR_BRIGHT_WHITE);
            char render_buf[INPUT_BUF_LEN + 16];
            str_strcpy(render_buf, input_buf);
            str_strcat(render_buf, "_   ");
            print_at(render_buf, 9, 20, COLOR_WHITE);

            needs_redraw = 0;
        }

        uint64_t key = get_key();

        if (key == KEY_CTRL_O) {
            show_help = !show_help;
            needs_redraw = 1;
        } else if (key == KEY_CTRL_N) {
            history_count = 0;
            latest_query[0] = '\0';
            str_strcpy(latest_result, "0.000000");
            input_len = 0;
            input_buf[0] = '\0';
            needs_redraw = 1;
        } else if (key == KEY_BACKSPACE) {
            if (input_len > 0) {
                input_len--;
                input_buf[input_len] = '\0';
                needs_redraw = 1;
            }
        } else if (key == KEY_ENTER || key == 0x0A || key == 0x0D) {
            if (input_len == 0) continue;

            if (str_strcmp(input_buf, "exit") == 0 || str_strcmp(input_buf, "quit") == 0) {
                sys_exit(0);
            }
            if (str_strcmp(input_buf, "clear") == 0 || str_strcmp(input_buf, "cls") == 0) {
                history_count = 0;
                latest_query[0] = '\0';
                str_strcpy(latest_result, "0.000000");
                input_len = 0;
                input_buf[0] = '\0';
                needs_redraw = 1;
                continue;
            }
            if (str_strcmp(input_buf, "mc") == 0) {
                memory_reg.int_part = 0;
                memory_reg.frac_part = 0;
                input_len = 0;
                input_buf[0] = '\0';
                needs_redraw = 1;
                continue;
            }
            if (str_strcmp(input_buf, "mr") == 0) {
                last_answer = memory_reg;
                input_len = 0;
                input_buf[0] = '\0';
                needs_redraw = 1;
                continue;
            }

            int err = 0;
            fixed_t res = evaluate(input_buf, last_answer, &err);

            char res_str[32];
            if (err == 1) {
                str_strcpy(res_str, "Syntax Error");
            } else if (err == 2) {
                str_strcpy(res_str, "Division by Zero");
            } else {
                last_answer = res;
                fixed_to_str(res.int_part, res.frac_part, res_str, 32);
            }

            str_strcpy(latest_query, input_buf);
            str_strcpy(latest_result, res_str);

            if (history_count < MAX_HISTORY) {
                for (int i = 0; i < INPUT_BUF_LEN; i++) history[history_count].query[i] = input_buf[i];
                for (int i = 0; i < 32; i++) history[history_count].result[i] = res_str[i];
                history_count++;
            } else {
                for (int i = 0; i < MAX_HISTORY - 1; i++) {
                    for (int k = 0; k < INPUT_BUF_LEN; k++) history[i].query[k] = history[i + 1].query[k];
                    for (int k = 0; k < 32; k++) history[i].result[k] = history[i + 1].result[k];
                }
                for (int i = 0; i < INPUT_BUF_LEN; i++) history[MAX_HISTORY - 1].query[i] = input_buf[i];
                for (int i = 0; i < 32; i++) history[MAX_HISTORY - 1].result[i] = res_str[i];
            }

            input_len = 0;
            input_buf[0] = '\0';
            needs_redraw = 1;
        } else if (key >= 32 && key < 127) {
            if (input_len < INPUT_BUF_LEN - 1) {
                input_buf[input_len++] = (char)key;
                input_buf[input_len] = '\0';
                needs_redraw = 1;
            }
        }
    }

    sys_exit(0);
}