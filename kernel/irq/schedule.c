
#include <irql.h>
#include <common.h>
#include <schedule.h>
#include <assert.h>
#include <spinlock.h>


// TODO: move to header
#define SCHEDULE_POLICY_FIXED             0
#define SCHEUDLE_POLICY_USER_HIGHER       1
#define SCHEDULE_POLICY_USER_NORMAL       2
#define SCHEDULE_POLICY_USER_LOWER        3

#define FIXED_PRIORITY_KERNEL_HIGH        0
#define FIXED_PRIORITY_KERNEL_NORMAL      30
#define FIXED_PRIORITY_IDLE               255

// TODO: add schedule_policy to struct thread.

static void uint64_t CalculateEndTimesliceTime(struct thread* thr) {
    if (thr->priority == 255) {
        return 0;
    }
    return (20 + thr->priority) * 1000000ULL;
}

static void UpdatePriority(struct thread* thr, bool yielded) {
    int policy = thr->schedule_policy;
    if (policy != SCHEDULE_POLICY_FIXED) {
        int min_val = policy == SCHEUDLE_POLICY_USER_HIGHER ? 50 : (policy == SCHEDULE_POLICY_USER_NORMAL ? 100 : 150);
        int max_val = min_val + 100;
        int new_val = thr->priority + (yielded ? -1 : 1);
        if (new_val >= min_val && new_val <= max_val) {
            thr->priority = new_val;
        }
    }
}

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
