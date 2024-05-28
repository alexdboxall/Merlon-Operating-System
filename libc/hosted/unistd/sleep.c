#include <time.h>
#include <unistd.h>

int usleep(useconds_t usec) {
    struct timespec req;
    req.tv_sec = usec / 1000000;
    req.tv_nsec = (usec % 1000000) * 1000ULL;
    return nanosleep(&req, NULL);
}

unsigned sleep(unsigned seconds) {
    struct timespec req;
    struct timespec rem;
    req.tv_sec = seconds;
    req.tv_nsec = 0;
    int res = nanosleep(&req, &rem);
    if (res == 0) {
        return 0;
    }
    uint64_t ns_remaining = req.tv_nsec + req.tv_sec * 1000000000ULL;
    int sec_remaining = (ns_remaining + 1000000000ULL - 1) / 1000000000ULL;
    return sec_remaining;
}