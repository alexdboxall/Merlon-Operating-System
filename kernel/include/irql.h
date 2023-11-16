#pragma once

/*
 * Scheduler works. Page faults are allowed.
 */
#define IRQL_STANDARD       0

/*
 * Scheduler still works at this point. Cannot page fault.
 */
#define IRQL_PAGE_FAULT     10

/*
 * This is the scheduler (and therefore things won't be scheduled out 'behind its back'). Cannot page fault. 
 */
#define IRQL_SCHEDULER      20

/*
 * Scheduling will be postponed. Cannot page fault. Cannot use lower-priority devices.
 */
#define IRQL_DRIVER         30      // 30...79 is the driver range

/*
 * No scheduling, no page faulting, no using other hardware devices.
 */
#define IRQL_TIMER          80      // 80...89 is the special range

/*
 * No interrupts from here.
 */
#define IRQL_INIT           92
#define IRQL_PANIC          95
#define IRQL_HIGH           99      // generic 'highest possible' 

void RunAtIrql(int irql, void(*handler)(void*), void* context);
int GetIrql(void);
int RaiseIrql(int level);
void LowerIrql(int level);