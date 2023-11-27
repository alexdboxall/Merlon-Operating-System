
#include <irql.h>
#include <common.h>
#include <schedule.h>
#include <assert.h>
#include <spinlock.h>

static struct spinlock scheduler_lock;

/*
 * Because these IRQL jumps need to persist across switches, we can't just chuck
 * it on the stack like normal. No need for nesting / a stack to store these, as the
 * scheduler lock cannot be nested.
 */
static int scheduler_lock_irql;

void LockScheduler(void) {
    scheduler_lock_irql = AcquireSpinlock(&scheduler_lock, true);
}

void UnlockScheduler(void) {
    ReleaseSpinlockAndLower(&scheduler_lock, scheduler_lock_irql);
}

void AssertSchedulerLockHeld(void) {
    assert(IsSpinlockHeld(&scheduler_lock));
}

void ScheduleWithLockHeld(void) {
    EXACT_IRQL(IRQL_SCHEDULER);
    AssertSchedulerLockHeld();
    
}

void Schedule(void) {
    // TODO: decide if a page fault handler should allow task switches or not (I would guess )
    if (GetIrql() != IRQL_STANDARD) {
        PostponeScheduleUntilStandardIrql();
        return;
    }

    LockScheduler();
    ScheduleWithLockHeld();
    UnlockScheduler();
}

void InitScheduler() {
    InitSpinlock(&scheduler_lock, "scheduler", IRQL_SCHEDULER);
}