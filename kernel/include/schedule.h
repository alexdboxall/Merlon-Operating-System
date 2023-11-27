#pragma once

#include <common.h>

void Schedule(void);
void ScheduleWithLockHeld(void);
void LockScheduler(void);
void UnlockScheduler(void);
void InitScheduler(void);
void AssertSchedulerLockHeld(void);