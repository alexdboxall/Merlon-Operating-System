#include <stdio.h>
#include <errno.h>
#include <string.h>

void perror(const char* s) {
    if (s && s[0]) {
        fprintf(stderr, "%s: %s\n", s, strerror(errno));
    } else {
        fprintf(stderr, "%s\n", strerror(errno));
    }
}