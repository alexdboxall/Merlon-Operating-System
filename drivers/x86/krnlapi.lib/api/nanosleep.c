
#include "krnlapi.h"
#include <time.h>
#include <errno.h>

int nanosleep(const struct timespec* req, struct timespec* rem) {
    if (req == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (req->tv_nsec < 0 || req->tv_nsec > 999999999LL) {
        errno = EINVAL;
        return -1;
    }

    uint64_t nanosecs = req->tv_nsec * 1000000000ULL + req->tv_sec;
    uint64_t remainder = 0;
    int res = _system_call(SYSCALL_NANOSLEEP, (size_t) &nanosecs, (size_t) &remainder, 0, 0, 0);
    if (rem != NULL) {
        rem->tv_nsec = remainder % 1000000000ULL;
        rem->tv_sec = remainder / 1000000000ULL;
    }
    return res;
}