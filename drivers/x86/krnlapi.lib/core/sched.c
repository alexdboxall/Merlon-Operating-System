#include "krnlapi.h"
#include <sched.h>
#include <errno.h>

void sched_yield(void) {
    _system_call(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

int sched_get_priority_max(int) {
    errno = ENOSYS;
    return -1;
}

int sched_get_priority_min(int) {
    errno = ENOSYS;
    return -1;
}

int sched_getparam(pid_t, struct sched_param*) {
    errno = ENOSYS;
    return -1;
}

int sched_getscheduler(pid_t) {
    return SCHED_OTHER;
}
//int sched_rr_get_interval(pid_t, struct timespec*);

int sched_setparam(pid_t, const struct sched_param*) {
    errno = ENOSYS;
    return -1;
}

int sched_setscheduler(pid_t, int policy, const struct sched_param*) {
    if (policy == SCHED_OTHER) {
        return SCHED_OTHER;
    }
    errno = EINVAL;
    return -1;
}