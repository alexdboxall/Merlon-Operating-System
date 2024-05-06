#include <locale.h>
#include <limits.h>
#include <string.h>

struct lconv c_locale = {
    .decimal_point = ".",
    .thousands_sep = "",
    .grouping = "",
    .mon_decimal_point = "",
    .mon_thousands_sep = "",
    .mon_grouping = "",
    .positive_sign = "",
    .negative_sign = "",
    .currency_symbol = "",
    .frac_digits = CHAR_MAX,
    .p_cs_precedes = CHAR_MAX,
    .n_cs_precedes = CHAR_MAX,
    .p_sep_by_space = CHAR_MAX,
    .n_sep_by_space = CHAR_MAX,
    .p_sign_posn = CHAR_MAX,
    .n_sign_posn = CHAR_MAX,
    .int_curr_symbol = "",
    .int_frac_digits = CHAR_MAX,
    .int_p_cs_precedes = CHAR_MAX,
    .int_n_cs_precedes = CHAR_MAX,
    .int_p_sep_by_space = CHAR_MAX,
    .int_n_sep_by_space = CHAR_MAX,
    .int_p_sign_posn = CHAR_MAX,
    .int_n_sign_posn = CHAR_MAX
};

struct lconv* localeconv(void) {
    return &c_locale;
}

char* setlocale(int category, const char* locale) {
    (void) category;

    /*
     * Passing NULL reads the locale without changing it.
     */
    if (locale == NULL) {
        return "C";
    }

    /*
     * We're only going to support the "C" locale - and the "default" locale
     * (i.e. when the empty string is passed in) is just going to be the C
     * locale.
     */
    if (locale[0] == 0 || !strcmp(locale, "C")) {
        return "C";

    } else {
        return NULL;
    }
}