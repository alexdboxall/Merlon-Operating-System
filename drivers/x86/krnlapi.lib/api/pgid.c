#include "krnlapi.h"
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

pid_t getpgid(pid_t pid) {
    if (pid == 0) {
        pid = getpid();
    }
    pid_t out;
    int res = _system_call(SYSCALL_PGID, 0, (size_t) &pid, (size_t) &out, 0, 0);
    if (res != 0) {
        errno = res;
        return -1;
    }
    return out;
}

int setpgid(pid_t pid, pid_t pgid) {
    if (pid == 0) {
        pid = getpid();
    }
    if (pgid == 0) {
        pgid = getpgid(pid);
    }
    int res = _system_call(SYSCALL_PGID, 1, (size_t) &pid, (size_t) &pgid, 0, 0);
    if (res != 0) {
        errno = res;
        return -1;
    }
    return 0;
}
