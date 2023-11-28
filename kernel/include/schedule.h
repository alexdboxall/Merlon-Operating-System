#pragma once

#include <common.h>

#define SCHEDULE_POLICY_FIXED             0
#define SCHEUDLE_POLICY_USER_HIGHER       1
#define SCHEDULE_POLICY_USER_NORMAL       2
#define SCHEDULE_POLICY_USER_LOWER        3

#define FIXED_PRIORITY_KERNEL_HIGH        0
#define FIXED_PRIORITY_KERNEL_NORMAL      30
#define FIXED_PRIORITY_IDLE               255

void Schedule(void);
void ScheduleWithLockHeld(void);
void LockScheduler(void);
void UnlockScheduler(void);
void InitScheduler(void);
void AssertSchedulerLockHeld(void);
