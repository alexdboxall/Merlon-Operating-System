#include "krnlapi.h"
#include <errno.h>

int close(int fd) {
    int res = _system_call(SYSCALL_CLOSE, fd, 0, 0, 0, 0);
    if (res == 0) {
        return 0;
    } else {
        errno = res;
        return -1;
    }
}