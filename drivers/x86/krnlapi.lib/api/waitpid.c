#include "krnlapi.h"
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

pid_t waitpid(pid_t pid, int* status, int options) {
    pid_t pid_out;
    int res = _system_call(SYSCALL_WAITPID, pid, (size_t) &pid_out, (size_t) status, options, 0);
    if (res != 0) {
        errno = res;
        return -1;
    }

    return pid_out;
}