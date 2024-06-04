#include "krnlapi.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

extern void fork_return_trampoline(void);

pid_t fork(void) {
    pid_t pid_out = 12345;
    pid_t pid_in = getpid();
    int res = _system_call(SYSCALL_FORK, (size_t) &pid_out, (size_t) fork_return_trampoline, 0, 0, 0);
    if (res == 0) {
        if (pid_in == pid_out) {
            return 0;
        } else {
            return pid_out;
        }
    } else {
        errno = res;
        return -1;
    }
}