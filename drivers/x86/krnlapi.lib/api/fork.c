#include "krnlapi.h"
#include <errno.h>
#include <fcntl.h>

pid_t fork(void) {
    pid_t pid_out;
    int res = _system_call(SYSCALL_FORK, (size_t) &pid_out, 0, 0, 0, 0);
    if (res == 0) {
        return pid_out;
    } else {
        errno = res;
        return -1;
    }
}