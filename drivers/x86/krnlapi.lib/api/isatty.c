#include "krnlapi.h"
#include <errno.h>

int isatty(int fd) {
    // TODO: implement this properly!!
    if (fd < 3) {
        return 1;
    }
    errno = ENOTTY;
    return 0;
}