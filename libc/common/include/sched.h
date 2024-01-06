#pragma once

#include <sys/types.h>

#define SCHED_FIFO      0
#define SCHED_RR        1
#define SCHED_SPORADIC  2
#define SCHED_OTHER     3

struct sched_param {
    int sched_priority;

};

void sched_yield(void);
int sched_get_priority_max(int);
int sched_get_priority_min(int);
int sched_getparam(pid_t, struct sched_param*);
int sched_getscheduler(pid_t);
//int sched_rr_get_interval(pid_t, struct timespec*);
int sched_setparam(pid_t, const struct sched_param*);
int sched_setscheduler(pid_t, int, const struct sched_param*);