
#include <thread.h>
#include <semaphore.h>
#include <threadlist.h>
#include <heap.h>
#include <errno.h>
#include <string.h>
#include <irql.h>
#include <timer.h>
#include <assert.h>
#include <panic.h>
#include <log.h>

struct semaphore {
    const char* name;
    int max_count;
    int current_count;
    struct thread_list waiting_list;
};

struct semaphore* CreateSemaphore(const char* name, int max_count, int initial_count) {
    MAX_IRQL(IRQL_SCHEDULER);

    struct semaphore* sem = AllocHeap(sizeof(struct semaphore));
    sem->name = name;
    sem->max_count = max_count;
    sem->current_count = initial_count;
    ThreadListInit(&sem->waiting_list, NEXT_INDEX_SEMAPHORE);
    return sem;
}

int FillSemaphore(struct semaphore* sem) {
    int res = 0;
    LockScheduler();
    if (sem->current_count <= sem->max_count) {
        res = sem->max_count - sem->current_count;
        sem->current_count = sem->max_count;
    } else {
        res = -1;
    }
    UnlockScheduler();
    return res;
}

/**
 * Acquires (i.e. does the waits or P operation on) a semaphore. The timeout is
 * given in milliseconds. If 0, we return without blocking - if the lock can't
 * be acquired, then EGAIN is returned. If -1, there is no timeout (may block 
 * indefinitely).
 * 
 * @return 0 if the lock if acquired. EAGAIN if timeout_ms is 0 and the lock
 *         can't be acquired. ETIMEDOUT if timeout_ms isn't 0, and we timed-out.
 */
int AcquireSemaphore(struct semaphore* sem, int timeout_ms) {
    if (GetIrql() != 0) {
        LogWriteSerial("[AcquireSemaphore]: irql %d, sem->name = %s\n", GetIrql(), sem->name);
    }
    EXACT_IRQL(IRQL_STANDARD);
    assert(sem != NULL);

    LockScheduler();

    struct thread* thr = GetThread();
    if (thr == NULL) {
        if (sem->current_count < sem->max_count) {
            sem->current_count++;
        } else {
            Panic(PANIC_SEM_BLOCK_WITHOUT_THREAD);
        }
        UnlockScheduler();
        return 0;
    }

    thr->timed_out = false;

    if (sem->current_count < sem->max_count) {
        sem->current_count++;
    } else {
        thr->waiting_on_semaphore = sem;

        if (timeout_ms == 0) {
            thr->timed_out = true;

        } else if (timeout_ms == -1) {
            ThreadListInsert(&sem->waiting_list, thr);
            BlockThread(THREAD_STATE_WAITING_FOR_SEMAPHORE);

        } else {
            ThreadListInsert(&sem->waiting_list, thr);
            thr->sleep_expiry = GetSystemTimer() + ((uint64_t) timeout_ms) * 1000ULL * 1000ULL;
            QueueForSleep(thr);
            BlockThread(THREAD_STATE_WAITING_FOR_SEMAPHORE_WITH_TIMEOUT);
        }
    } 

    UnlockScheduler();
    return thr->timed_out ? (timeout_ms == 0 ? EAGAIN : ETIMEDOUT) : 0;
}

static void DecrementSemaphore(struct semaphore* sem, bool first) {
    if (sem->waiting_list.head == NULL) {
        if (sem->current_count == 0) {
            Panic(PANIC_NEGATIVE_SEMAPHORE);
        }
        sem->current_count--;

    } else {
        struct thread* top = ThreadListDeleteTop(&sem->waiting_list);

        if (top->state == THREAD_STATE_WAITING_FOR_SEMAPHORE_WITH_TIMEOUT) {
            /*
            * If it's in this state, it means it's either on the ready queue, 
            * after just having finisherd a sleep, OR it's on the sleep queue.
            * 
            * If it's on the sleep queue, we need to remove it from there.
            * Otherwise, we do nothing, as it's already on the ready queue.
            */
            bool on_sleep_queue = TryDequeueForSleep(top);
            if (on_sleep_queue) {
                /*
                * Change the state to prevent UnblockThread from seeing it's in
                * the timeout state and calling CancelSemaphoreOfThread.
                */
                top->state = THREAD_STATE_READY;
                if (first) {
                    UnblockThreadGiftingTimeslice(top);
                } else {
                    UnblockThread(top);
                }
            }

        } else {
            if (first) {
                UnblockThreadGiftingTimeslice(top);
            } else {
                UnblockThread(top);
            }
        }
    }
}

int ReleaseSemaphoreEx(struct semaphore* sem, int count) {
    if (count < 0) {
        return 0;
    }
    
    assert(sem->current_count > 0);
    int decrements = 0;
    do {
        ++decrements;
        DecrementSemaphore(sem, decrements == 0);
    } while (--count && sem->current_count > 0);

    return decrements;
}

/**
 * Releases (i.e., does the signal, or V operation on) a semaphore.
 */
void ReleaseSemaphore(struct semaphore* sem) {
    MAX_IRQL(IRQL_HIGH);

    LockScheduler();
    ReleaseSemaphoreEx(sem, 1);
    UnlockScheduler();
}

/**
 * Deallocates a semaphore. Flags are one of SEM_DONT_CARE, SEM_REQUIRE_ZERO or 
 * SEM_REQUIRE_FULL, and EBUSY will be returned (instead of 0) if the semaphore 
 * is not in this state.
 */
int DestroySemaphore(struct semaphore* sem, int flags) {
    MAX_IRQL(IRQL_SCHEDULER);

    int ret = 0;
    LockScheduler();
    if ((flags == SEM_REQUIRE_ZERO && sem->current_count != 0) ||
        (flags == SEM_REQUIRE_FULL && sem->current_count != sem->max_count)) {
        ret = EBUSY;
    } else {
        FreeHeap(sem);
    }
    UnlockScheduler();
    return ret;
}

/**
 * Used to cancel a semaphore that has been timed out, by removing the thread 
 * from the semaphore wait list. If this doesn't occur, the sleep wakeup routine
 * will allow the thread to continue running, which will lead to a crash if that
 * thread then attempts to acquire the same semaphore.
 */
void CancelSemaphoreOfThread(struct thread* thr) {
    AssertSchedulerLockHeld();
    assert(ThreadListContains(&thr->waiting_on_semaphore->waiting_list, thr));
    ThreadListDelete(&thr->waiting_on_semaphore->waiting_list, thr);
}
