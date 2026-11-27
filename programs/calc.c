#include "../dogec/dogeio.h"
#include "../dogec/stdio.h"
#include "../dogec/string.h"
#include "../dogec/math.h"

#define FX_SCALE     1000000LL
#define FX_HALF      500000LL
#define FX_PI        3141593LL
#define FX_E         2718282LL
#define FX_TWO_PI    6283185LL
#define FX_HALF_PI   1570796LL
#define FX_TRIG_LIM  10LL

static int64_t c_abs_i64(int64_t x) {
    return (x < 0) ? -x : x;
}

static uint64_t c_to_u64_mag(int64_t x, int *neg) {
    if (x < 0) {
        if (neg) *neg = !(*neg);
        return (uint64_t)(-(x + 1)) + 1ULL;
    }
    return (uint64_t)x;
}

static int64_t c_from_u64_mag(uint64_t mag, int neg) {
    return neg ? -(int64_t)mag : (int64_t)mag;
}

static int64_t c_mul(int64_t a, int64_t b) {
    int neg = 0;
    uint64_t ua = c_to_u64_mag(a, &neg);
    uint64_t ub = c_to_u64_mag(b, &neg);

    uint64_t a_hi = ua / 1000000ULL;
    uint64_t a_lo = ua % 1000000ULL;
    uint64_t b_hi = ub / 1000000ULL;
    uint64_t b_lo = ub % 1000000ULL;

    uint64_t lo_prod = a_lo * b_lo + 500000ULL;
    uint64_t res = (a_hi * b_hi) * 1000000ULL
                 + a_hi * b_lo
                 + a_lo * b_hi
                 + lo_prod / 1000000ULL;

    return c_from_u64_mag(res, neg);
}

static int c_div(int64_t a, int64_t b, int64_t *out) {
    if (!out || b == 0) return 0;

    int neg = 0;
    uint64_t ua = c_to_u64_mag(a, &neg);
    uint64_t ub = c_to_u64_mag(b, &neg);

    uint64_t q = ua / ub;
    uint64_t rem = ua % ub;
    uint64_t frac = 0;

    for (int i = 0; i < 6; i++) {
        rem *= 10ULL;
        frac = frac * 10ULL + (rem / ub);
        rem %= ub;
    }

    if (rem * 2ULL >= ub) {
        frac++;
        if (frac >= 1000000ULL) {
            frac -= 1000000ULL;
            q++;
        }
    }

    *out = c_from_u64_mag(q * 1000000ULL + frac, neg);
    return 1;
}

static int c_pow(int64_t base, int64_t exp_fx, int64_t *out) {
    if (!out) return 0;
    if (exp_fx % FX_SCALE != 0 || exp_fx < 0) return 0;

    uint64_t uexp = (uint64_t)(exp_fx / FX_SCALE);
    if (uexp == 0ULL) {
        *out = FX_SCALE;
        return 1;
    }

    int64_t acc = FX_SCALE;
    int64_t cur = base;

    while (uexp > 0ULL) {
        if (uexp & 1ULL) {
            acc = c_mul(acc, cur);
        }
        uexp >>= 1ULL;
        if (uexp > 0ULL) {
            cur = c_mul(cur, cur);
        }
    }

    *out = acc;
    return 1;
}

static int c_sqrt(int64_t x, int64_t *out) {
    if (!out || x < 0) return 0;
    if (x == 0) {
        *out = 0;
        return 1;
    }

    int64_t guess = (x > FX_SCALE) ? (x / 2) : x;
    if (guess <= 0) guess = 1;

    for (int iter = 0; iter < 32; iter++) {
        int64_t quot = 0;
        if (!c_div(x, guess, &quot)) return 0;
        int64_t next = (guess + quot) / 2;
        if (c_abs_i64(next - guess) <= 1) {
            guess = next;
            break;
        }
        guess = next;
    }

    *out = guess;
    return 1;
}

static int64_t c_wrap_pi(int64_t x) {
    int64_t r = x % FX_TWO_PI;
    if (r > FX_PI) r -= FX_TWO_PI;
    if (r < -FX_PI) r += FX_TWO_PI;
    return r;
}

static int c_sin(int64_t x, int64_t *out) {
    if (!out) return 0;
    int64_t r = c_wrap_pi(x);
    int64_t r2 = c_mul(r, r);
    int64_t term = r;
    int64_t sum = r;

    for (int n = 1; n <= 4; n++) {
        int64_t denom = (int64_t)((2 * n) * (2 * n + 1));
        term = -(c_mul(term, r2) / denom);
        sum += term;
    }

    *out = sum;
    return 1;
}

static int c_cos(int64_t x, int64_t *out) {
    if (!out) return 0;
    int64_t r = c_wrap_pi(x);
    int64_t r2 = c_mul(r, r);
    int64_t term = FX_SCALE;
    int64_t sum = FX_SCALE;

    for (int n = 1; n <= 4; n++) {
        int64_t denom = (int64_t)((2 * n - 1) * (2 * n));
        term = -(c_mul(term, r2) / denom);
        sum += term;
    }

    *out = sum;
    return 1;
}

static int c_tan(int64_t x, int64_t *out) {
    if (!out) return 0;
    int64_t s = 0;
    int64_t c = 0;
    if (!c_sin(x, &s) || !c_cos(x, &c)) return 0;
    if (c_abs_i64(c) < FX_TRIG_LIM) return 0;
    return c_div(s, c, out);
}

static void c_print_digits(uint64_t val) {
    char digits[24];
    int idx = 0;

    if (val == 0ULL) {
        printf("%s", "0");
        return;
    }

    while (val > 0ULL && idx < (int)sizeof(digits)) {
        digits[idx++] = (char)('0' + (val % 10ULL));
        val /= 10ULL;
    }

    for (int i = idx - 1; i >= 0; i--) {
        char out[2] = { digits[i], '\0' };
        printf("%s", out);
    }
}

static void c_print_val(int64_t val) {
    if (val < 0) {
        printf("%s", "-");
        val = -val;
    }

    uint64_t u = (uint64_t)val;
    uint64_t whole = u / 1000000ULL;
    uint64_t frac = u % 1000000ULL;

    c_print_digits(whole);
    printf("%s", ".");

    uint64_t place = 100000ULL;
    while (place > 0ULL) {
        uint64_t d = frac / place;
        char out[2] = { (char)('0' + d), '\0' };
        printf("%s", out);
        frac %= place;
        place /= 10ULL;
    }
    printf("\n");
}

typedef struct {
    const char *text;
    int idx;
    int err;
} ExprReader;

static void skip_spaces(ExprReader *r) {
    if (!r || !r->text) return;
    while (r->text[r->idx] == ' ' || r->text[r->idx] == '\t' ||
           r->text[r->idx] == '\n' || r->text[r->idx] == '\r') {
        r->idx++;
    }
}

static int name_eq(const char *s, const char *lit, int n) {
    if (!s || !lit || n < 0) return 0;
    for (int i = 0; i < n; i++) {
        char a = s[i];
        char b = lit[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
        if (a != b || a == '\0') return 0;
    }
    return lit[n] == '\0';
}

static int read_sum(ExprReader *r, int64_t *out);

static int call_builtin(const char *name, int len, int64_t arg, int64_t *out) {
    if (!out) return 0;
    if (name_eq(name, "sin", len))  return c_sin(arg, out);
    if (name_eq(name, "cos", len))  return c_cos(arg, out);
    if (name_eq(name, "tan", len))  return c_tan(arg, out);
    if (name_eq(name, "sqrt", len)) return c_sqrt(arg, out);
    if (name_eq(name, "abs", len))  { *out = c_abs_i64(arg); return 1; }
    return 0;
}

static int read_atom(ExprReader *r, int64_t *out) {
    if (!r || !r->text || !out) return 0;
    *out = 0;
    skip_spaces(r);
    char ch = r->text[r->idx];

    if (ch == '(') {
        r->idx++;
        int64_t inside = 0;
        if (!read_sum(r, &inside)) return 0;
        skip_spaces(r);
        if (r->text[r->idx] != ')') {
            printf("Error: Missing ')'\n");
            r->err = 1;
            return 0;
        }
        r->idx++;
        *out = inside;
        return 1;
    }

    if ((ch >= '0' && ch <= '9') || ch == '.') {
        int64_t ipart = 0;
        int64_t fpart = 0;
        int64_t fmult = 100000;
        int seen = 0;

        while (r->text[r->idx] >= '0' && r->text[r->idx] <= '9') {
            ipart = ipart * 10 + (int64_t)(r->text[r->idx] - '0');
            r->idx++;
            seen = 1;
        }

        if (r->text[r->idx] == '.') {
            r->idx++;
            while (r->text[r->idx] >= '0' && r->text[r->idx] <= '9') {
                int d = r->text[r->idx] - '0';
                if (fmult > 0) {
                    fpart += (int64_t)d * fmult;
                    fmult /= 10;
                }
                r->idx++;
                seen = 1;
            }
        }

        if (!seen) {
            printf("Error: Invalid number\n");
            r->err = 1;
            return 0;
        }

        *out = ipart * FX_SCALE + fpart;
        return 1;
    }

    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
        int start = r->idx;
        while ((r->text[r->idx] >= 'a' && r->text[r->idx] <= 'z') ||
               (r->text[r->idx] >= 'A' && r->text[r->idx] <= 'Z') ||
               (r->text[r->idx] >= '0' && r->text[r->idx] <= '9') ||
               r->text[r->idx] == '_') {
            r->idx++;
        }

        int len = r->idx - start;
        const char *ident = r->text + start;

        if (name_eq(ident, "pi", len)) { *out = FX_PI; return 1; }
        if (name_eq(ident, "e", len))  { *out = FX_E;  return 1; }

        skip_spaces(r);
        if (r->text[r->idx] == '(') {
            r->idx++;
            int64_t arg = 0;
            if (!read_sum(r, &arg)) return 0;
            skip_spaces(r);
            if (r->text[r->idx] != ')') {
                printf("Error: Missing ')' in call\n");
                r->err = 1;
                return 0;
            }
            r->idx++;

            int64_t ans = 0;
            if (!call_builtin(ident, len, arg, &ans)) {
                printf("Error: Bad function or domain\n");
                r->err = 1;
                return 0;
            }
            *out = ans;
            return 1;
        }

        printf("Error: Unknown symbol\n");
        r->err = 1;
        return 0;
    }

    printf("Error: Unexpected token\n");
    r->err = 1;
    return 0;
}

static int read_sign(ExprReader *r, int64_t *out) {
    if (!r || !r->text || !out) return 0;
    skip_spaces(r);

    if (r->text[r->idx] == '+') {
        r->idx++;
        return read_sign(r, out);
    }

    if (r->text[r->idx] == '-') {
        r->idx++;
        int64_t v = 0;
        if (!read_sign(r, &v)) return 0;
        *out = -v;
        return 1;
    }

    return read_atom(r, out);
}

static int read_exp(ExprReader *r, int64_t *out) {
    if (!r || !r->text || !out) return 0;
    int64_t base = 0;
    if (!read_sign(r, &base)) return 0;

    skip_spaces(r);
    if (r->text[r->idx] == '^') {
        r->idx++;
        int64_t exp_fx = 0;
        if (!read_exp(r, &exp_fx)) return 0;

        int64_t res = 0;
        if (!c_pow(base, exp_fx, &res)) {
            printf("Error: Invalid exponent\n");
            r->err = 1;
            return 0;
        }
        *out = res;
        return 1;
    }

    *out = base;
    return 1;
}

static int read_prod(ExprReader *r, int64_t *out) {
    if (!r || !r->text || !out) return 0;
    int64_t cur = 0;
    if (!read_exp(r, &cur)) return 0;

    while (1) {
        skip_spaces(r);
        char op = r->text[r->idx];
        if (op == '*') {
            r->idx++;
            int64_t rhs = 0;
            if (!read_exp(r, &rhs)) return 0;
            cur = c_mul(cur, rhs);
        } else if (op == '/') {
            r->idx++;
            int64_t rhs = 0;
            if (!read_exp(r, &rhs)) return 0;
            int64_t q = 0;
            if (!c_div(cur, rhs, &q)) {
                printf("Error: Division by zero\n");
                r->err = 1;
                return 0;
            }
            cur = q;
        } else {
            break;
        }
    }

    *out = cur;
    return 1;
}

static int read_sum(ExprReader *r, int64_t *out) {
    if (!r || !r->text || !out) return 0;
    int64_t cur = 0;
    if (!read_prod(r, &cur)) return 0;

    while (1) {
        skip_spaces(r);
        char op = r->text[r->idx];
        if (op == '+') {
            r->idx++;
            int64_t rhs = 0;
            if (!read_prod(r, &rhs)) return 0;
            cur += rhs;
        } else if (op == '-') {
            r->idx++;
            int64_t rhs = 0;
            if (!read_prod(r, &rhs)) return 0;
            cur -= rhs;
        } else {
            break;
        }
    }

    *out = cur;
    return 1;
}

static int eval_line(char *line, int64_t *ans) {
    if (!line || !ans) return 0;
    ExprReader r;
    r.text = line;
    r.idx = 0;
    r.err = 0;

    int64_t val = 0;
    if (!read_sum(&r, &val)) return 0;
    skip_spaces(&r);

    if (!r.err && r.text[r.idx] != '\0') {
        printf("Error: Extra input\n");
        r.err = 1;
        return 0;
    }

    *ans = val;
    return 1;
}

static void trim_eol(char *s) {
    if (!s) return;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            s[i] = '\0';
            break;
        }
    }
}

void _start(int argc, char **argv) {
    char *expr = (void *)0;

    if (argv != (void *)0) {
        if (argc >= 2 && argv[1] != (void *)0 && argv[1][0] != '\0') {
            expr = argv[1];
        } else if (argc == 1 && argv[0] != (void *)0 && argv[0][0] != '\0' && strcmp(argv[0], "calc") != 0) {
            expr = argv[0];
        }
    }

    if (expr == (void *)0) {
        printf("Calculator (type 'exit' to quit)\n");
        char buf[256];
        while (1) {
            printf("> ");
            if (fgets(buf, sizeof(buf), stdin) == (void *)0) {
                break;
            }
            trim_eol(buf);
            if (strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0) {
                break;
            }
            if (buf[0] == '\0') {
                continue;
            }
            int64_t ans = 0;
            if (eval_line(buf, &ans)) {
                printf("= ");
                c_print_val(ans);
            }
        }
        exit();
        return;
    }

    if (strcmp(expr, "--help") == 0) {
        printf("Usage: calc [expression]\n");
        printf("Funcs: sin, cos, tan, sqrt, abs\n");
        printf("Consts: pi, e\n");
        exit();
        return;
    }

    int64_t ans = 0;
    if (eval_line(expr, &ans)) {
        printf("Result: ");
        c_print_val(ans);
    }

    exit();
}