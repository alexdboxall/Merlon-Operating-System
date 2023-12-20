#pragma once

/**
 * SIMPLE TABLE
 *
 *                  Can page fault?     Can task switch?    Can use drivers?    Can have IRQs?
 * IRQL_STANDARD    YES                 YES                 YES                 YES
 * IRQL_PAGE_FAULT  SORT OF             YES                 YES                 YES                     (only the page fault handler can generate a nested page fault, e.g. handling some COW stuff)
 * IRQL_SCHEDULER   NO                  SORT OF             YES                 YES                     (only the scheduler can make a task switch occur, others get postponed)
 * IRQL_DRIVER      NO                  NO                  SORT OF             YES                     (only higher priority drivers can be used)
 * IRQL_TIMER       NO                  NO                  NO                  NO                      (but the timer handler jumps up to this level)                  
 * IRQL_HIGH        NO                  NO                  NO                  NO
 *
 */

/*
 * Scheduler works. Page faults are allowed.
 */
#define IRQL_STANDARD       0

/*
 * Scheduler still works at this point. Cannot page fault.
 */
#define IRQL_PAGE_FAULT     1

/*
 * This is the scheduler (and therefore things won't be scheduled out 'behind its back'). Cannot page fault. 
 */
#define IRQL_SCHEDULER      2

/*
 * Scheduling will be postponed. Cannot page fault. Cannot use lower-priority devices.
 */
#define IRQL_DRIVER         3      // 3...39 is the driver range

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

void InitIrql(void);

#define MAX_IRQL(l) assert(GetIrql() <= l)
#define MIN_IRQL(l) assert(GetIrql() >= l)
#define EXACT_IRQL(l) assert(GetIrql() == l)