#include <spinlock.h>
#include <string.h>
#include <arch.h>
#include <irql.h>
#include <panic.h>
#include <log.h>
#include <assert.h>

void InitSpinlock(struct spinlock* lock, const char* name, int irql) {
    assert(strlen(name) <= 15);
    assert(irql >= IRQL_SCHEDULER);
    lock->lock = 0;
    lock->irql = irql;
    strcpy(lock->name, name);
}

int AcquireSpinlock(struct spinlock* lock) {
    if (lock->lock != 0) {
        PanicEx(PANIC_SPINLOCK_DOUBLE_ACQUISITION, lock->name);
    }

    /* 
     * It's okay to take a spinlock to a higher level, and this is used for
     * things like, e.g. releasing a semaphore from an interrupt handler.
     */
    int prior_irql = RaiseIrql(lock->irql);
    if (prior_irql < lock->irql) {
        RaiseIrql(lock->irql);
    }

    ArchSpinlockAcquire(&lock->lock);
    lock->prev_irql = prior_irql;
    return prior_irql;
}

void ReleaseSpinlock(struct spinlock* lock) {
    if (lock->lock == 0) {
        PanicEx(PANIC_SPINLOCK_RELEASED_BEFORE_ACQUIRED, lock->name);
    }

    int old_irql = lock->prev_irql;
    ArchSpinlockRelease(&lock->lock);
    LowerIrql(old_irql);
}

/**
 * This function has no atomic guarantees. It should only be used for debugging 
 * and writing assertion statements. 
 */
bool IsSpinlockHeld(struct spinlock* lock) {
    return lock->lock;
}
