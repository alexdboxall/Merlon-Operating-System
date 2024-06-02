#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

/*
 * No need to set `errno` here very often, as `ioctl` already does that for us.
 */

int tcgetattr(int fd, struct termios* term) {
    return ioctl(fd, TCGETS, term);
}

int tcsetattr(int fd, int optional_actions, const struct termios* term) {
    if (optional_actions != TCSANOW) {
        errno = ENOSYS;
        return -1;
    }

    return ioctl(fd, TCSETS, term);   
}

pid_t tcgetpgrp(int fd) {
    pid_t pgid;
    int res = ioctl(fd, TIOCGPGRP, &pgid);
    if (res == -1) {
        return -1;
    }
    return pgid;
}

int tcsetpgrp(int fd, pid_t pgrp) {
    return ioctl(fd, TIOCSPGRP, &pgrp);
}