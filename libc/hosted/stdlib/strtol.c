#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>
#include <_stdckdint.h>

static bool IsDigitInBase(char c, int base) {
    if (isdigit(c) && (c - '0') < base) {
        return true;
    }
    return tolower(c) - 'a' + 10 < base;
}

#define STRTO(TYPE, NAME, LIM_MIN, LIM_MAX, UNSIGNED) \
TYPE NAME (\
    const char * restrict str, char ** restrict endptr, int base\
) {\
    if (base < 2 || base > 36) {\
        errno = EINVAL;\
        return 0;\
    }\
\
    TYPE val = 0;\
\
    int i = 0;\
    while (str[i] && isspace(str[i])) {\
        ++i;\
    }\
\
    bool negate = false;\
    if (str[i] == '+') {\
        ++i;\
    } else if (str[i] == '-') {\
        negate = true;\
        ++i;\
    }\
\
    if (str[i] == '0' && (str[i] == 'x' || str[i] == 'X') && (base == 0 || base == 16)) {\
        base = 16;\
        str += 2;\
    } else if (str[i] == '0' && base == 0) {\
        base = 8;\
        str++;\
    } else if (base == 0) {\
        base = 10;\
    }\
\
    TYPE dummy;\
    bool any_digits = false;\
    while (str[i] && IsDigitInBase(str[i], base)) {\
        any_digits = true;\
        val *= base;\
\
        int digit_val = isdigit(str[i]) ? (str[i] - '0') : (tolower(str[i]) - 'a' + 10);\
\
        if (negate) {\
            if (UNSIGNED && digit_val > 0) {\
                errno = ERANGE;\
                return 0;\
            }\
            if (ckd_sub(&dummy, val, digit_val)) {\
                errno = ERANGE;\
                return LIM_MIN;\
            }\
            val -= digit_val;\
        } else {\
            if (ckd_add(&dummy, val, digit_val)) {\
                errno = ERANGE;\
                return LIM_MAX;\
            }\
            val += digit_val;\
        }\
        ++i;\
    }\
\
    if (endptr != NULL) {\
        *endptr = (char*) (any_digits ? str + i : str);\
    }\
\
    return val;\
}

STRTO(long int, strtol, LONG_MIN, LONG_MAX, false)
STRTO(long long int, strtoll, LLONG_MIN, LLONG_MAX, false)
STRTO(unsigned long int, strtoul, 0, ULONG_MAX, true)
STRTO(unsigned long long int, strtoull, 0, ULLONG_MAX, true)
