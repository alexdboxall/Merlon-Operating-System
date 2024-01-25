
#include <timer.h>
#include <spinlock.h>
#include <assert.h>
#include <cpu.h>
#include <panic.h>
#include <irql.h>
#include <log.h>
#include <arch.h>
#include <thread.h>
#include <priorityqueue.h>
#include <threadlist.h>

static struct spinlock timer_lock;
static struct thread_list sleep_list;

static uint64_t system_time = 0;
static int sleep_wakeups_posted = 0;

void ReceivedTimer(uint64_t nanos) {
    EXACT_IRQL(IRQL_TIMER);

    if (ArchGetCurrentCpuIndex() == 0) {
        /*
         * Although we are in IRQL_TIMER, we must still lock to prevent other 
         * CPUs from reading at a bad time. 
         */
        AcquireSpinlock(&timer_lock);
        system_time += nanos;
        ReleaseSpinlock(&timer_lock);
    }

    /*
     * Preempt the current thread if it has used up its timeslice. 
     */
    struct thread* thr = GetThread();
    if (thr != NULL && thr->timeslice_expiry != 0 && thr->timeslice_expiry <= system_time) {
        PostponeScheduleUntilStandardIrql();
    }

    if (GetNumberInDeferQueue() < 8) {
        DeferUntilIrql(IRQL_STANDARD, HandleSleepWakeups, (void*) &system_time);
    }
}

uint64_t GetSystemTimer(void) {    
    AcquireSpinlock(&timer_lock);
    uint64_t value = system_time;
    ReleaseSpinlock(&timer_lock);
    return value;
}

void InitTimer(void) {
    InitSpinlock(&timer_lock, "timer", IRQL_TIMER);
    ThreadListInit(&sleep_list, NEXT_INDEX_SLEEP);
}

void QueueForSleep(struct thread* thr) {
    AssertSchedulerLockHeld();
    thr->timed_out = false;
    ThreadListInsert(&sleep_list, thr);
}

bool TryDequeueForSleep(struct thread* thr) {
    AssertSchedulerLockHeld();
    
    struct thread* iter = sleep_list.head;
    while (iter) {
        if (iter == thr) {
            ThreadListDelete(&sleep_list, iter);
            return true;

        } else {
            iter = iter->next[NEXT_INDEX_SLEEP];
        }
    }

    return false;
}

void HandleSleepWakeups(void* sys_time_ptr) {
    EXACT_IRQL(IRQL_STANDARD);

    if (GetThread() == NULL) {
        return;
    }

    LockScheduler();
    if (sleep_wakeups_posted > 0) {
        --sleep_wakeups_posted;
    }

    uint64_t system_time = *((uint64_t*) sys_time_ptr);

    struct thread* iter = sleep_list.head;
    while (iter) {
        if (iter->sleep_expiry <= system_time) {
            ThreadListDelete(&sleep_list, iter);
            iter->timed_out = true;
            UnblockThread(iter);
            iter = sleep_list.head;     // restart, as list changed

        } else {
            iter = iter->next[NEXT_INDEX_SLEEP];
        }
    }

    UnlockScheduler();
}

void SleepUntil(uint64_t system_time_ns) {
    EXACT_IRQL(IRQL_STANDARD);

    if (system_time_ns < GetSystemTimer()) {
        return;
    }

    LockScheduler();
    GetThread()->sleep_expiry = system_time_ns;
    QueueForSleep(GetThread());
    BlockThread(THREAD_STATE_SLEEPING);
    UnlockScheduler();
}

void SleepNano(uint64_t delta_ns) {
    SleepUntil(GetSystemTimer() + delta_ns);
}

void SleepMilli(uint32_t delta_ms) {
    SleepNano(((uint64_t) delta_ms) * 1000000ULL);
}
