#include "krnlapi.h"
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

int isatty(int fd) {
    struct termios term;
    int res = ioctl(fd, TCGETS, &term);
    if (res == 0) {
        return 1;
    } else {
        return 0;
    }
}