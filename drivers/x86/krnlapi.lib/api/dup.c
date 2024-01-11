#include "krnlapi.h"
#include <errno.h>

static int dup_cleanup(int res, int newfd) {
    if (res == 0) {
        return newfd;
    } else {
        errno = res;
        return -1;
    }
}

int dup(int oldfd) {
    size_t newfd;
    int res = _system_call(SYSCALL_DUP, 1, oldfd, (size_t) &newfd, 0, 0);
    return dup_cleanup(res, newfd);
}

int dup2(int oldfd, int newfd) {
    return dup_cleanup(_system_call(SYSCALL_DUP, 2, oldfd, newfd, 0, 0), newfd);
}

int dup3(int oldfd, int newfd, int flags) {
    if (oldfd == newfd) {
        errno = EINVAL;
        return -1;
    }
    return dup_cleanup(_system_call(SYSCALL_DUP, 2, oldfd, newfd, flags, 0), newfd);
}
