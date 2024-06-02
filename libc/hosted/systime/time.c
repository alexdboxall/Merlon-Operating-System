#include <sys/time.h>
#include <merlon/time.h>
#include <errno.h>
#include <timeconv.h>

int gettimeofday(struct timeval* tv, struct timezone* restrict tz) {
    (void) tz;

    if (tv != NULL) {
        uint64_t t = OsGetLocalTime();
        if (t == 0) {
            errno = EIO;
            return -1;
        }
        tv->tv_sec = TimeValueToUnixTime(t);
        tv->tv_usec = t % 1000000;
    }

    return 0;
}

int settimeofday(const struct timeval* tv, const struct timezone* tz) {
    (void) tz;

    uint64_t t = UnixTimeToTimeValue(tv->tv_sec);
    t += tv->tv_usec;

    int res = OsSetLocalTime(t);
    if (res != 0) {
        errno = res;
        return -1;
    }

    return 0;
}
