#pragma once

#include <sys/types.h>

#ifndef NULL
#define NULL	((void*) 0)
#endif

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

#define CLOCKS_PER_SEC 1000000

time_t time(time_t* t);
clock_t clock(void);
char* asctime(const struct tm* timeptr);
struct tm* localtime(const time_t* timer);
char* ctime(const time_t* clock);