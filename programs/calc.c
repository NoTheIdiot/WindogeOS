#include "../dogec/dogeio.h"
#include "../dogec/stdio.h"
#include "../dogec/string.h"
#include "../dogec/math.h"

#define SCI_PI 3.14159265358979323846
#define SCI_E  2.71828182845904523536

static void sci_abs(double x, double* out) {
    *out = (x < 0.0) ? -x : x;
}

static void sci_pow(double base, double exp, double* out) {
    if (exp == 0.0) {
        *out = 1.0;
        return;
    }

    long long iexp = (long long)exp;
    if ((double)iexp == exp) {
        int neg = 0;
        unsigned long long uexp;
        if (iexp < 0) {
            neg = 1;
            uexp = (unsigned long long)(-(iexp + 1)) + 1ULL;
        } else {
            uexp = (unsigned long long)iexp;
        }

        double result = 1.0;
        double cur = base;
        while (uexp > 0ULL) {
            if (uexp & 1ULL) {
                result *= cur;
            }
            uexp >>= 1ULL;
            if (uexp > 0ULL) {
                cur *= cur;
            }
        }
        *out = neg ? (1.0 / result) : result;
        return;
    }

    if (base < 0.0) {
        *out = 0.0 / 0.0;
        return;
    }
    if (base == 0.0) {
        *out = (exp > 0.0) ? 0.0 : (1.0 / 0.0);
        return;
    }

    *out = (double)math_power((uint64_t)base, (int)exp);
}

static void sci_sqrt(double x, double* out) {
    if (x < 0.0) {
        *out = 0.0 / 0.0;
        return;
    }
    if (x == 0.0) {
        *out = 0.0;
        return;
    }

    double guess = x;
    if (x > 1.0) {
        guess = x * 0.5;
    }
    for (int i = 0; i < 40; i++) {
        guess = 0.5 * (guess + x / guess);
    }
    *out = guess;
}

static void sci_exp(double x, double* out) {
    if (x > 709.0) {
        *out = 1.0 / 0.0;
        return;
    }
    if (x < -745.0) {
        *out = 0.0;
        return;
    }

    const double LN2 = 0.69314718055994530942;
    long long k = (long long)((x >= 0.0) ? (x / LN2 + 0.5) : (x / LN2 - 0.5));
    double r = x - (double)k * LN2;

    double term = 1.0;
    double sum = 1.0;
    for (int n = 1; n <= 25; n++) {
        term *= r / (double)n;
        sum += term;
    }

    double p2 = 1.0;
    sci_pow(2.0, (double)k, &p2);
    *out = sum * p2;
}

static void sci_ln(double x, double* out) {
    if (x <= 0.0) {
        *out = 0.0 / 0.0;
        return;
    }

    int k = 0;
    while (x > 1.5) {
        x *= 0.5;
        k++;
    }
    while (x < 0.75) {
        x *= 2.0;
        k--;
    }

    double u = (x - 1.0) / (x + 1.0);
    double u2 = u * u;
    double term = u;
    double sum = u;
    for (int n = 3; n <= 35; n += 2) {
        term *= u2;
        sum += term / (double)n;
    }

    const double LN2 = 0.69314718055994530942;
    *out = (2.0 * sum) + ((double)k * LN2);
}

static void sci_log10(double x, double* out) {
    const double LN10 = 2.30258509299404568402;
    double ln_x = 0.0;
    sci_ln(x, &ln_x);
    *out = ln_x / LN10;
}

static void wrap_angle_rad(double x, double* out) {
    const double TWO_PI = 2.0 * SCI_PI;
    long long cycles = (long long)(x / TWO_PI);
    double r = x - (double)cycles * TWO_PI;
    if (r > SCI_PI) r -= TWO_PI;
    if (r < -SCI_PI) r += TWO_PI;
    *out = r;
}

static void sci_sin(double x, double* out) {
    double r = 0.0;
    wrap_angle_rad(x, &r);
    double r2 = r * r;
    double term = r;
    double sum = r;
    for (int n = 1; n <= 12; n++) {
        term *= -r2 / (double)((2 * n) * (2 * n + 1));
        sum += term;
    }
    *out = sum;
}

static void sci_cos(double x, double* out) {
    double r = 0.0;
    wrap_angle_rad(x, &r);
    double r2 = r * r;
    double term = 1.0;
    double sum = 1.0;
    for (int n = 1; n <= 12; n++) {
        term *= -r2 / (double)((2 * n - 1) * (2 * n));
        sum += term;
    }
    *out = sum;
}

static void sci_tan(double x, double* out) {
    double c = 0.0;
    double s = 0.0;
    double abs_c = 0.0;
    sci_cos(x, &c);
    sci_abs(c, &abs_c);
    if (abs_c < 1e-15) {
        *out = 0.0 / 0.0;
        return;
    }
    sci_sin(x, &s);
    *out = s / c;
}

static int sci_isnan(double x) {
    return x != x;
}

static int sci_isinf(double x) {
    return !sci_isnan(x) && (x > 1e308 || x < -1e308);
}

static void print_unsigned_ll(unsigned long long val) {
    char buf[32];
    int idx = 0;
    if (val == 0ULL) {
        printf("0");
        return;
    }
    while (val > 0ULL) {
        buf[idx++] = (char)('0' + (val % 10ULL));
        val /= 10ULL;
    }
    for (int i = idx - 1; i >= 0; i--) {
        char ch[2] = { buf[i], '\0' };
        printf(ch);
    }
}

void print_double(double val) {
    if (sci_isnan(val)) {
        printf("NaN\n");
        return;
    }
    if (sci_isinf(val)) {
        printf(val < 0.0 ? "-inf\n" : "inf\n");
        return;
    }
    if (val < 0.0) {
        printf("-");
        val = -val;
    }

    double scaled = val * 1000000.0 + 0.5;
    unsigned long long total_micro = (unsigned long long)scaled;
    unsigned long long whole = total_micro / 1000000ULL;
    unsigned long long frac = total_micro % 1000000ULL;

    print_unsigned_ll(whole);
    printf(".");

    unsigned long long div = 100000ULL;
    while (div > 0ULL) {
        unsigned long long digit = frac / div;
        char ch[2] = { (char)('0' + digit), '\0' };
        printf(ch);
        frac %= div;
        div /= 10ULL;
    }
    printf("\n");
}

typedef struct {
    const char* src;
    int pos;
    int error;
} Parser;

static void skip_ws(Parser* p) {
    while (p->src[p->pos] == ' ' || p->src[p->pos] == '\t' ||
           p->src[p->pos] == '\n' || p->src[p->pos] == '\r') {
        p->pos++;
    }
}

static int str_ieq(const char* a, const char* b, int len) {
    for (int i = 0; i < len; i++) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if (ca != cb || ca == '\0') return 0;
    }
    return b[len] == '\0';
}

static void parse_expr(Parser* p, double* out);

static void apply_func(const char* name, int len, double arg, double* out) {
    if (str_ieq(name, "sin", len))  { sci_sin(arg, out);   return; }
    if (str_ieq(name, "cos", len))  { sci_cos(arg, out);   return; }
    if (str_ieq(name, "tan", len))  { sci_tan(arg, out);   return; }
    if (str_ieq(name, "sqrt", len)) { sci_sqrt(arg, out);  return; }
    if (str_ieq(name, "exp", len))  { sci_exp(arg, out);   return; }
    if (str_ieq(name, "ln", len))   { sci_ln(arg, out);    return; }
    if (str_ieq(name, "log", len))  { sci_log10(arg, out); return; }
    if (str_ieq(name, "abs", len))  { sci_abs(arg, out);   return; }
    *out = 0.0 / 0.0;
}

static void parse_primary(Parser* p, double* out) {
    *out = 0.0;
    skip_ws(p);
    char c = p->src[p->pos];

    if (c == '(') {
        p->pos++;
        double val = 0.0;
        parse_expr(p, &val);
        skip_ws(p);
        if (p->src[p->pos] != ')') {
            printf("Error: Missing ')'\n");
            p->error = 1;
            return;
        }
        p->pos++;
        *out = val;
        return;
    }

    if ((c >= '0' && c <= '9') || c == '.') {
        double val = 0.0;
        int has_digit = 0;
        while (p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
            val = val * 10.0 + (double)(p->src[p->pos] - '0');
            p->pos++;
            has_digit = 1;
        }
        if (p->src[p->pos] == '.') {
            p->pos++;
            double place = 0.1;
            while (p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
                val += (double)(p->src[p->pos] - '0') * place;
                place *= 0.1;
                p->pos++;
                has_digit = 1;
            }
        }
        if (!has_digit) {
            printf("Error: Invalid number\n");
            p->error = 1;
            return;
        }
        if (p->src[p->pos] == 'e' || p->src[p->pos] == 'E') {
            int save_pos = p->pos;
            p->pos++;
            int exp_sign = 1;
            if (p->src[p->pos] == '+') {
                p->pos++;
            } else if (p->src[p->pos] == '-') {
                exp_sign = -1;
                p->pos++;
            }
            if (p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
                int exp_val = 0;
                while (p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
                    exp_val = exp_val * 10 + (p->src[p->pos] - '0');
                    p->pos++;
                }
                double scale = 1.0;
                sci_pow(10.0, (double)(exp_sign * exp_val), &scale);
                val *= scale;
            } else {
                p->pos = save_pos;
            }
        }
        *out = val;
        return;
    }

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        int start = p->pos;
        while ((p->src[p->pos] >= 'a' && p->src[p->pos] <= 'z') ||
               (p->src[p->pos] >= 'A' && p->src[p->pos] <= 'Z') ||
               (p->src[p->pos] >= '0' && p->src[p->pos] <= '9') ||
               p->src[p->pos] == '_') {
            p->pos++;
        }
        int len = p->pos - start;
        const char* ident = p->src + start;

        if (str_ieq(ident, "pi", len)) { *out = SCI_PI; return; }
        if (str_ieq(ident, "e", len))  { *out = SCI_E;  return; }

        skip_ws(p);
        if (p->src[p->pos] == '(') {
            p->pos++;
            double arg = 0.0;
            parse_expr(p, &arg);
            skip_ws(p);
            if (p->src[p->pos] != ')') {
                printf("Error: Missing ')' in function call\n");
                p->error = 1;
                return;
            }
            p->pos++;
            double res = 0.0;
            apply_func(ident, len, arg, &res);
            if (sci_isnan(res)) {
                printf("Error: Math domain error\n");
                p->error = 1;
                return;
            }
            *out = res;
            return;
        }

        printf("Error: Unknown identifier\n");
        p->error = 1;
        return;
    }

    printf("Error: Unexpected character\n");
    p->error = 1;
}

static void parse_unary(Parser* p, double* out) {
    *out = 0.0;
    skip_ws(p);
    if (p->src[p->pos] == '+') {
        p->pos++;
        parse_unary(p, out);
        return;
    }
    if (p->src[p->pos] == '-') {
        p->pos++;
        double val = 0.0;
        parse_unary(p, &val);
        *out = -val;
        return;
    }
    parse_primary(p, out);
}

static void parse_power(Parser* p, double* out) {
    *out = 0.0;
    double base = 0.0;
    parse_unary(p, &base);
    if (p->error) return;

    skip_ws(p);
    if (p->src[p->pos] == '^') {
        p->pos++;
        double exp = 0.0;
        parse_power(p, &exp);
        if (p->error) return;
        double res = 0.0;
        sci_pow(base, exp, &res);
        if (sci_isnan(res)) {
            printf("Error: Math domain error\n");
            p->error = 1;
            return;
        }
        *out = res;
        return;
    }
    *out = base;
}

static void parse_term(Parser* p, double* out) {
    *out = 0.0;
    double val = 0.0;
    parse_power(p, &val);
    if (p->error) return;

    while (1) {
        skip_ws(p);
        char op = p->src[p->pos];
        if (op == '*') {
            p->pos++;
            double rhs = 0.0;
            parse_power(p, &rhs);
            if (p->error) return;
            val *= rhs;
        } else if (op == '/') {
            p->pos++;
            double rhs = 0.0;
            parse_power(p, &rhs);
            if (p->error) return;
            if (rhs == 0.0) {
                printf("Error: Division by zero\n");
                p->error = 1;
                return;
            }
            val /= rhs;
        } else {
            break;
        }
    }
    *out = val;
}

static void parse_expr(Parser* p, double* out) {
    *out = 0.0;
    double val = 0.0;
    parse_term(p, &val);
    if (p->error) return;

    while (1) {
        skip_ws(p);
        char op = p->src[p->pos];
        if (op == '+') {
            p->pos++;
            double rhs = 0.0;
            parse_term(p, &rhs);
            if (p->error) return;
            val += rhs;
        } else if (op == '-') {
            p->pos++;
            double rhs = 0.0;
            parse_term(p, &rhs);
            if (p->error) return;
            val -= rhs;
        } else {
            break;
        }
    }
    *out = val;
}

int evaluate(char* string, double* result) {
    if (string == (void*)0 || result == (void*)0) {
        return 0;
    }
    Parser p;
    p.src = string;
    p.pos = 0;
    p.error = 0;

    double val = 0.0;
    parse_expr(&p, &val);
    skip_ws(&p);
    if (!p.error && p.src[p.pos] != '\0') {
        printf("Error: Unexpected trailing input\n");
        p.error = 1;
    }
    if (p.error) {
        return 0;
    }

    *result = val;
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
        printf("Scientific Calculator (type 'exit' to quit)\n");
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
            double res = 0.0;
            if (evaluate(input, &res)) {
                printf("= ");
                print_double(res);
            }
        }
        exit();
        return;
    }

    if (strcmp(expr, "--help") == 0) {
        printf("Scientific Calculator\n");
        printf("Usage: calc [expression]\n");
        printf("Functions: sin, cos, tan, sqrt, exp, ln, log, abs\n");
        printf("Constants: pi, e\n");
        printf("Example: calc \"sin(pi / 2) + 2 ^ 3 * sqrt(16)\"\n");
        exit();
        return;
    }

    double res = 0.0;
    if (evaluate(expr, &res)) {
        printf("Result: ");
        print_double(res);
    }

    exit();
}