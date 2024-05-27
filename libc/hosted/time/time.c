#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <timeconv.h>
#include <sys/time.h>
#include <os/time.h>

/*
 * crt0 to initialise this.
 */
uint64_t initial_time_for_clock;

clock_t clock(void) {
    return (OsGetLocalTime() - initial_time_for_clock);
}

time_t time(time_t* t) {
    struct timeval tv;
    int res = gettimeofday(&tv, NULL);
    if (res != 0) {
        return (time_t)(-1);
    }
    if (t != NULL) {
        *t = tv.tv_sec;
    }
    return tv.tv_sec;
}

char* asctime(const struct tm* timeptr) {
    static char wday_name[7][3] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static char mon_name[12][3] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static char result[26];
    sprintf(result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
        wday_name[timeptr->tm_wday],
        mon_name[timeptr->tm_mon],
        timeptr->tm_mday, timeptr->tm_hour,
        timeptr->tm_min, timeptr->tm_sec,
        1900 + timeptr->tm_year);
    return result;
}

struct tm* localtime(const time_t* timer) {
    if (timer == NULL) {
        errno = EINVAL;
        return NULL;
    }

    uint64_t t = UnixTimeToTimeValue(*timer);
    int days_since_1601 = t / 1000000ULL / SECS_PER_DAY;
    struct ostime ost = TimeValueToStruct(t);
    
    static struct tm tm;
    tm.tm_sec = ost.sec;
    tm.tm_min = ost.min;
    tm.tm_hour = ost.hour;
    tm.tm_mday = ost.day;
    tm.tm_mon = ost.month - 1;
    tm.tm_year = ost.year - 1900;
    tm.tm_yday = cumulative_days_in_months[ost.month - 1] + ost.day - 1;
	tm.tm_wday = (1 + days_since_1601) % 7;
    tm.tm_isdst = 0;
    return &tm;
}

struct tm* gmtime(const time_t* timer) {
    uint64_t t = (OsGetLocalTime() - OsGetUtcTime()) / 1000000ULL;
    t = *timer - t;
    return localtime(&t);
}

char* ctime(const time_t* clock) {
    return asctime(localtime(clock));
}