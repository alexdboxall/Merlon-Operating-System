
#include <signal.h>
#include <errno.h>

void (*signal(int sig, void (*func)(int)))(int) {
    (void) sig;
    (void) func;

    errno = ENOSYS;
    return SIG_ERR;
}

int raise(int sig) {
    (void) sig;
    return ENOSYS;
}

int kill(pid_t pid, int sig) {
    (void) pid;
    (void) sig;
    errno = ENOSYS;
    return -1;
}