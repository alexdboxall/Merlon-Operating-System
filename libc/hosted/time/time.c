#include <time.h>
#include <stdio.h>
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