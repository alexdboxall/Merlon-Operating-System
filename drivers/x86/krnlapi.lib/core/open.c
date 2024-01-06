#include "krnlapi.h"
#include <errno.h>
#include <fcntl.h>

int open(const char* filename, int flags, mode_t mode) {
    size_t fd;
    int res = _system_call(SYSCALL_OPEN, (size_t) filename, flags, mode, (size_t) &fd, 0);
    if (res == 0) {
        return fd;
    } else {
        errno = res;
        return -1;
    }
}