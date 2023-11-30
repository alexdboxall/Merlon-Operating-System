
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
static struct priority_queue* sleep_queue;
static struct thread_list sleep_overflow_list;

#define SLEEP_QUEUE_LENGTH 32

static uint64_t system_time = 0;

void ReceivedTimer(uint64_t nanos) {
    EXACT_IRQL(IRQL_TIMER);

    if (ArchGetCurrentCpuIndex() == 0) {
        /*
         * As we're in the timer handler, we know we already have IRQL_TIMER, and so we don't
         * need to incur the additional overhead of raising and lowering.
         */
        AcquireSpinlock(&timer_lock, false);
        system_time += nanos;
        ReleaseSpinlock(&timer_lock);
    }

    /*
     * Preempt the current thread if it has used up its timeslice. 
     * TODO: check if this still is correct for SMP. 
     */
    if (GetThread() != NULL && GetThread()->timeslice_expiry != 0 && GetThread()->timeslice_expiry <= system_time) {
        PostponeScheduleUntilStandardIrql();
    }

    DeferUntilIrql(IRQL_STANDARD, HandleSleepWakeups, (void*) &system_time);
}

uint64_t GetSystemTimer(void) {
    MAX_IRQL(IRQL_TIMER);

    int irql = AcquireSpinlock(&timer_lock, true);
    uint64_t value = system_time;
    ReleaseSpinlockAndLower(&timer_lock, irql);
    return value;
}

void InitTimer(void) {
    InitSpinlock(&timer_lock, "timer", IRQL_TIMER);
    ThreadListInit(&sleep_overflow_list, NEXT_INDEX_SLEEP);
    sleep_queue = PriorityQueueCreate(SLEEP_QUEUE_LENGTH, false, sizeof(struct thread*));
}

void QueueForSleep(struct thread* thr) {
    thr->timed_out = false;

    if (PriorityQueueGetUsedSize(sleep_queue) == SLEEP_QUEUE_LENGTH) {
        ThreadListInsert(&sleep_overflow_list, thr);
    } else {
        PriorityQueueInsert(sleep_queue, (void*) &thr, thr->sleep_expiry);
    }
}

void DequeueForSleep(struct thread* thr) {
    while (PriorityQueueGetUsedSize(sleep_queue) > 0) {
        struct priority_queue_result res = PriorityQueuePeek(sleep_queue);
        struct thread* top_thread = *((struct thread**) res.data);
        PriorityQueuePop(sleep_queue);
        if (top_thread == thr) {
            return;
        }
        ThreadListInsert(&sleep_overflow_list, top_thread);
    }

    struct thread* iter = sleep_overflow_list.head;
    while (iter) {
        if (iter == thr) {
            ThreadListDelete(&sleep_overflow_list, iter);
            return;

        } else {
            iter = iter->next[NEXT_INDEX_SLEEP];
        }
    }

    Panic(PANIC_IMPOSSIBLE_RETURN);
}

void HandleSleepWakeups(void* sys_time_ptr) {
    EXACT_IRQL(IRQL_STANDARD);

    if (GetThread() == NULL) {
        return;
    }

    LockScheduler();

    uint64_t system_time = *((uint64_t*) sys_time_ptr);

    /*
     * Wake up any sleeping tasks that need it.
     */
    while (PriorityQueueGetUsedSize(sleep_queue) > 0) {
        struct priority_queue_result res = PriorityQueuePeek(sleep_queue);
        
        /*
         * Check if it needs waking.
         */
        if (res.priority <= system_time) {
            struct thread* thr = *((struct thread**) res.data);
            thr->timed_out = true;
            UnblockThread(thr);
            PriorityQueuePop(sleep_queue);
        } else {
            /*
             * If this one doesn't need waking, none of the others will either.
             */
            break;
        }
    }

    /*
     * Check for any tasks that are asleep but on the overflow list. This is slow, but will
     * only happen if we have more than 32 sleeping tasks, so it should normally not take any time.
     */
    struct thread* iter = sleep_overflow_list.head;
    while (iter) {
        if (iter->sleep_expiry <= system_time) {
            ThreadListDelete(&sleep_overflow_list, iter);
            iter->timed_out = true;
            UnblockThread(iter);
            iter = sleep_overflow_list.head;            // restart, as list changed

        } else {
            iter = iter->next[NEXT_INDEX_SLEEP];
        }
    }

    UnlockScheduler();
}

void SleepUntil(uint64_t system_time_ns) {
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