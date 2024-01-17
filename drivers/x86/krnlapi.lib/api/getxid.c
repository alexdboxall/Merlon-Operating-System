#include "krnlapi.h"
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

pid_t getpid(void) {
    pid_t out;
    _system_call(SYSCALL_GETPID, (size_t) &out, 0, 0, 0, 0);
    return out;
}

pid_t getppid(void) {
    pid_t out;
    _system_call(SYSCALL_GETPID, (size_t) &out, 1, 0, 0, 0);
    return out;
}

pid_t gettid(void) {
    return (pid_t) _system_call(SYSCALL_GETTID, 0, 0, 0, 0, 0);
}
