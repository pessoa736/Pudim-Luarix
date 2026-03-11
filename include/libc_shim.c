#include "utypes.h"
#include "stddef.h"

int abs(int x) {
    return (x < 0) ? -x : x;
}

void* memcpy(void* dest, const void* src, size_t n) {
    size_t i;
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}

void* memset(void* dest, int c, size_t n) {
    size_t i;
    uint8_t* d = (uint8_t*)dest;

    for (i = 0; i < n; i++) {
        d[i] = (uint8_t)c;
    }

    return dest;
}

int memcmp(const void* a, const void* b, size_t n) {
    size_t i;
    const uint8_t* aa = (const uint8_t*)a;
    const uint8_t* bb = (const uint8_t*)b;

    for (i = 0; i < n; i++) {
        if (aa[i] != bb[i]) {
            return (aa[i] < bb[i]) ? -1 : 1;
        }
    }

    return 0;
}

size_t strlen(const char* s) {
    size_t n = 0;

    while (s[n]) {
        n++;
    }

    return n;
}

char* strcpy(char* dest, const char* src) {
    size_t i = 0;

    while (src[i]) {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
    return dest;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && (*a == *b)) {
        a++;
        b++;
    }

    if (*a == *b) {
        return 0;
    }

    return ((unsigned char)*a < (unsigned char)*b) ? -1 : 1;
}

int strncmp(const char* a, const char* b, size_t n) {
    size_t i;

    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return ((unsigned char)a[i] < (unsigned char)b[i]) ? -1 : 1;
        }
        if (a[i] == '\0') {
            return 0;
        }
    }

    return 0;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }

    if (c == 0) {
        return (char*)s;
    }

    return NULL;
}

char* strpbrk(const char* s, const char* accept) {
    while (*s) {
        const char* a = accept;
        while (*a) {
            if (*s == *a) {
                return (char*)s;
            }
            a++;
        }
        s++;
    }
    return NULL;
}

size_t strspn(const char* s, const char* accept) {
    size_t n = 0;

    while (*s) {
        const char* a = accept;
        int found = 0;

        while (*a) {
            if (*s == *a) {
                found = 1;
                break;
            }
            a++;
        }

        if (!found) {
            break;
        }

        n++;
        s++;
    }

    return n;
}

int strcoll(const char* a, const char* b) {
    return strcmp(a, b);
}

static int is_digit(char c) {
    return (c >= '0' && c <= '9');
}

double strtod(const char* nptr, char** endptr) {
    const char* p = nptr;
    int neg = 0;
    double val = 0.0;
    double frac = 0.1;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }

    if (*p == '-') {
        neg = 1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    while (is_digit(*p)) {
        val = (val * 10.0) + (double)(*p - '0');
        p++;
    }

    if (*p == '.') {
        p++;
        while (is_digit(*p)) {
            val += (double)(*p - '0') * frac;
            frac *= 0.1;
            p++;
        }
    }

    if (endptr) {
        *endptr = (char*)p;
    }

    return neg ? -val : val;
}

double floor(double x) {
    long long i = (long long)x;

    if ((double)i > x) {
        i--;
    }

    return (double)i;
}

double fmod(double x, double y) {
    if (y == 0.0) {
        return 0.0;
    }

    return x - floor(x / y) * y;
}

double pow(double x, double y) {
    long long n = (long long)y;
    double r = 1.0;
    long long i;

    if ((double)n != y) {
        return 1.0;
    }

    if (n < 0) {
        if (x == 0.0) {
            return 0.0;
        }
        n = -n;
        for (i = 0; i < n; i++) {
            r *= x;
        }
        return 1.0 / r;
    }

    for (i = 0; i < n; i++) {
        r *= x;
    }

    return r;
}

double ldexp(double x, int exp) {
    if (exp > 0) {
        while (exp--) {
            x *= 2.0;
        }
    } else {
        while (exp++) {
            x *= 0.5;
        }
    }

    return x;
}

double frexp(double x, int* exp) {
    double m = x;
    int e = 0;

    if (m == 0.0) {
        *exp = 0;
        return 0.0;
    }

    while (m >= 1.0 || m <= -1.0) {
        m *= 0.5;
        e++;
    }

    while ((m > 0.0 && m < 0.5) || (m < 0.0 && m > -0.5)) {
        m *= 2.0;
        e--;
    }

    *exp = e;
    return m;
}

struct lconv {
    char* decimal_point;
    char* thousands_sep;
    char* grouping;
    char* int_curr_symbol;
    char* currency_symbol;
    char* mon_decimal_point;
    char* mon_thousands_sep;
    char* mon_grouping;
    char* positive_sign;
    char* negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
};

struct lconv* localeconv(void) {
    static struct lconv lc;
    static char dot[2] = ".";

    lc.decimal_point = dot;
    lc.thousands_sep = dot;
    lc.grouping = dot;

    return &lc;
}

int snprintf(char* str, size_t size, const char* fmt, ...) {
    size_t i = 0;
    size_t j = 0;

    if (size == 0) {
        return 0;
    }

    while (fmt[i] && j + 1 < size) {
        if (fmt[i] == '%' && fmt[i + 1]) {
            if (fmt[i + 1] == '%') {
                str[j++] = '%';
                i += 2;
                continue;
            }
            str[j++] = '?';
            i += 2;
            continue;
        }

        str[j++] = fmt[i++];
    }

    str[j] = '\0';
    return (int)j;
}

int _setjmp(void* env) {
    (void)env;
    return 0;
}

void longjmp(void* env, int val) {
    (void)env;
    (void)val;

    while (1) {
        asm volatile ("hlt");
    }
}

void abort(void) {
    while (1) {
        asm volatile ("hlt");
    }
}
