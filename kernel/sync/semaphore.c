
#include <thread.h>
#include <semaphore.h>
#include <threadlist.h>
#include <heap.h>
#include <errno.h>
#include <irql.h>
#include <timer.h>
#include <assert.h>
#include <panic.h>
#include <log.h>


struct semaphore {
    int max_count;
    int current_count;
    struct thread_list waiting_list;
};

/**
 * Creates a semaphore object with a specified limit on the number of concurrent holders.
 * 
 * @param max_count The maximum number of concurrent holders of the semaphore
 * @param initial_count The initial number of holders of the semaphore. Should normally be 0.
 * @returns The initialised semaphore. 
 * 
 * @maxirql IRQL_STANDARD
 */
struct semaphore* CreateSemaphore(int max_count, int initial_count) {
    MAX_IRQL(IRQL_STANDARD);

    struct semaphore* sem = AllocHeap(sizeof(struct semaphore));
    sem->max_count = max_count;
    sem->current_count = initial_count;
    ThreadListInit(&sem->waiting_list, NEXT_INDEX_SEMAPHORE);
    return sem;
}

/**
 * Acquires (i.e. does the waits or P operation on) a semaphore. This operation my block depending on the timeout value.
 * 
 * @param sem           The semaphore to acquire.
 * @param timeout_ms    One of either:
 *                           0: Attempt to acquire the semaphore, but will not block if it cannot be acquired.
 *                          -1: Will acquire semaphore, even if it needs to block to do so. Will not timeout.
 *                          +N: Same as -1, except that the operation will timeout after the specified number of
 *                              milliseconds.
 * 
 * @return 0 if the semaphore was acquired
 *         ETIMEDOUT if the semaphore was not acquired, and the operation timed out
 *         EAGAIN if the semaphore was not acquired, and the timeout_ms value was 0
 * 
 * @maxirql IRQL_SCHEDULER
 */
int AcquireSemaphore(struct semaphore* sem, int timeout_ms) {
    MAX_IRQL(IRQL_SCHEDULER);
    assert(sem != NULL);

    LockScheduler();

    struct thread* thr = GetThread();
    if (thr == NULL) {
        Panic(PANIC_SEM_HOLD_WITHOUT_THREAD);
        return 0;
    }

    /*
     * This gets set to true by the sleep wakeup routine if we get timed-out.
     */
    thr->timed_out = false;

    if (sem->current_count < sem->max_count) {
        /*
         * Uncontested, so acquire straight away.
         */
        sem->current_count++;
    } else {
        /*
         * Need to block for the semaphore (or return if the timeout is zero).
         */
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

/**
 * Releases (i.e., does the signal, or V operation on) a semaphore. If there are threads waiting on this semaphore,
 * it will cause the first one to wake up. 
 * 
 * @param sem The semaphore to release/signal 
 */
void ReleaseSemaphore(struct semaphore* sem) {
    MAX_IRQL(IRQL_SCHEDULER);

    LockScheduler();
    
    if (sem->waiting_list.head == NULL) {
        sem->current_count--;

    } else {
        struct thread* top = ThreadListDeleteTop(&sem->waiting_list);

        /*
         * If it's in the THREAD_STATE_WAITING_FOR_SEMAPHORE_WITH_TIMEOUT state, it could mean one of two things:
         *      - it's still on the sleep queue, in which case we need to get it off that queue, and put it on the ready queue
         *      - it's been taken off the sleep queue and onto the ready already, but it hasn't yet been run yet (and is therefore
         *        still in this state)
         */
        if (top->state == THREAD_STATE_WAITING_FOR_SEMAPHORE_WITH_TIMEOUT) {
            bool on_sleep_queue = TryDequeueForSleep(top);

            if (on_sleep_queue) {
                /*
                 * Change the state to prevent UnblockThread from seeing it's in the timeout state and calling CancelSemaphoreOfThread.
                 * If CancelSemaphoreOfThread were called, then it would attempt to delete it from the queue - but it's already been
                 * deleted by this point.
                 */
                top->state = THREAD_STATE_READY;
                UnblockThread(top);
            }
            /*
             * Do not unblock the thread if it's not on the sleep queue, as not being on the sleep queue means it's
             * already on the ready queue.
             */

        } else {
            UnblockThread(top);
        }
    }
    UnlockScheduler();
}

/**
 * Deallocates a semaphore. Loses its shit if someone is still holding onto it, as it is probably a bug if you're trying
 * to destroy a semaphore when there's a possiblity that someone might even be thinking about trying to acquire it (which
 * would then try to acquire a deleted memory region, which is very bad).
 * 
 * @param sem The semaphore to destroy.
 * 
 * @maxirql IRQL_SCHEDULER
 */
void DestroySemaphore(struct semaphore* sem) {
    MAX_IRQL(IRQL_SCHEDULER);

    LockScheduler();
    if (sem->current_count != 0) {
        Panic(PANIC_SEMAPHORE_DESTROY_WHILE_HELD);
    }
    FreeHeap(sem);
    UnlockScheduler();
}

/**
 * Internal function. Used by the sleep wakeup routine to cancel a semaphore that has been timed-out. 
 * Removes the thread from the semaphore wait list. If this does not occur, then the sleep wakeup routine will allow
 * the thread to continue running, which will lead to a crash if that thread then attempts to acquire the same semaphore.
 * Without this, stress tests will crash.
 */
void CancelSemaphoreOfThread(struct thread* thr) {
    AssertSchedulerLockHeld();
    assert(ThreadListContains(&thr->waiting_on_semaphore->waiting_list, thr));
    ThreadListDelete(&thr->waiting_on_semaphore->waiting_list, thr);
}

int GetSemaphoreCount(struct semaphore* sem) {
    return sem->current_count;
}