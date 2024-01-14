#pragma once

#include <assert.h>

/*
 * Scheduler works. Page faults are allowed.
 */
#define IRQL_STANDARD       0
#define IRQL_STANDARD_HIGH_PRIORITY 1

/*
 * Scheduler still works at this point. Cannot page fault.
 */
#define IRQL_PAGE_FAULT     2

/*
 * This is the scheduler (and therefore things won't be scheduled out 'behind its back'). Cannot page fault. 
 */
#define IRQL_SCHEDULER      3

/*
 * Scheduling will be postponed. Cannot page fault. Cannot use lower-priority devices.
 */
#define IRQL_DRIVER         4      // 3...39 is the driver range

/*
 * No scheduling, no page faulting, no using other hardware devices (no other irqs)
 */
#define IRQL_TIMER          40

/*
 * No interrupts from here.
 */
#define IRQL_HIGH           41

#include <common.h>
#include <assert.h>

void PostponeScheduleUntilStandardIrql(void);
void DeferUntilIrql(int irql, void(*handler)(void*), void* context);
int GetIrql(void);
int RaiseIrql(int level);
void LowerIrql(int level);
int GetNumberInDeferQueue(void);

void InitIrql(void);

#define MAX_IRQL(l) assert(GetIrql() <= l)
#define MIN_IRQL(l) assert(GetIrql() >= l)
#define EXACT_IRQL(l) assert(GetIrql() == l)