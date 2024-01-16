#include <ctype.h>

int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        c -= 32;
    }
    return c;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        c += 32;
    }
    return c;
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isalpha(int c) {
    return isupper(c) || islower(c);
}

int iscntrl(int c) {
    return c < 32 || c == 127;
}

int isblank(int c) {
    return c == '\t' || c == ' ';
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isgraph(int c) {
    return isprint(c) && c != ' ';
}

int islower(int c) {
    return c >= 'a' && c <= 'z';
}

int isprint(int c) {
    return !iscntrl(c);
}

int ispunct(int c) {
    /*
     * In the "C" locale, ispunct returns true for every printing character for 
     * which neither isspace nor isalnum is true.
     */
    return isprint(c) && !isspace(c) && !isalnum(c);
}

int isspace(int c) {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int isxdigit(int c) {
    return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}