#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

int atoi(const char *str) {
    int i = 0;
    while (str[i] && isspace(str[i])) {
        ++i;
    }

    bool negate = false;
    if (str[i] == '+') {
        ++i;
    } else if (str[i] == '-') {
        negate = true;
        ++i;
    }

    int val = 0;
    while (str[i] && isdigit(str[i])) {
        val *= 10;
        val += str[i] - '0';
        ++i;
    } 
    return negate ? -val : val;
}
