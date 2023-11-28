#include <_partial_/stdlib.h>
#include <stdint.h>


div_t div(int numer, int denom) {
    div_t r;
    r.quot = numer / denom;
    r.rem = numer % denom;
    return r;
}

ldiv_t ldiv(long int numer, long int denom) {
    ldiv_t r;
    r.quot = numer / denom;
    r.rem = numer % denom;
    return r;
}

lldiv_t lldiv(long long int numer, long long int denom) {
    lldiv_t r;
    r.quot = numer / denom;
    r.rem = numer % denom;
    return r;
}
