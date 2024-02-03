#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>

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