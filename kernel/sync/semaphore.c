
#include <thread.h>
#include <semaphore.h>
#include <threadlist.h>
#include <heap.h>
#include <errno.h>
#include <timer.h>
#include <panic.h>
#include <log.h>

struct semaphore {
    int max_count;
    int current_count;
    struct thread_list waiting_list;
};

struct semaphore* CreateSemaphore(int max_count) {
    struct semaphore* sem = AllocHeap(sizeof(struct semaphore));
    sem->max_count = max_count;
    sem->current_count = 0;
    ThreadListInit(&sem->waiting_list, NEXT_INDEX_SEMAPHORE);
    return sem;
}

/* timeout: -1 = infinite (i.e. will block), 0 = tryacquire, 1+ = tryacquire with timeout*/
int AcquireSemaphore(struct semaphore* sem, int timeout_ms) {
    LockScheduler();

    struct thread* thr = GetThread();
    thr->timed_out = false;
    
    if (sem->current_count < sem->max_count) {
        sem->current_count++;
    } else {
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

void ReleaseSemaphore(struct semaphore* sem) {
    LockScheduler();
    if (sem->waiting_list.head == NULL) {
        sem->current_count--;
    } else {
        struct thread* top = ThreadListDeleteTop(&sem->waiting_list);

        if (top->state == THREAD_STATE_WAITING_FOR_SEMAPHORE_WITH_TIMEOUT && !top->timed_out) {
            DequeueForSleep(top);
        }

        UnblockThread(top);
    }
    UnlockScheduler();
}

void DestroySemaphore(struct semaphore* sem) {
    LockScheduler();
    if (sem->current_count != 0) {
        Panic(PANIC_SEMAPHORE_DESTROY_WHILE_HELD);
    }
    FreeHeap(sem);
    UnlockScheduler();
}