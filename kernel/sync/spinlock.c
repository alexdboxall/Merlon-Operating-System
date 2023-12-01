#include <spinlock.h>
#include <string.h>
#include <arch.h>
#include <irql.h>
#include <panic.h>
#include <log.h>
#include <thread.h>
#include <assert.h>

void InitSpinlock(struct spinlock* lock, const char* name, int irql) {
    assert(strlen(name) <= 15);

    if (irql < IRQL_SCHEDULER) {
        Panic(PANIC_SPINLOCK_WRONG_IRQL);
    }

    lock->lock = 0;
    lock->owner = NULL;
    lock->irql = irql;
    strcpy(lock->name, name);
}

void AcquireSpinlockDirect(struct spinlock* lock) {
    if (lock->lock != 0) {
        LogWriteSerial("OOPS! %s\n", lock->name);
        Panic(PANIC_ASSERTION_FAILURE);
    }
    assert(lock->lock == 0);

    ArchSpinlockAcquire(&lock->lock);
    //lock->owner = GetThread();
}

void ReleaseSpinlockDirect(struct spinlock* lock) {
    assert(lock->lock != 0);
    //assert(lock->owner == GetThread() || lock->irql == IRQL_HIGH);
    //lock->owner = NULL;
    ArchSpinlockRelease(&lock->lock);
}

/**
 * This function has no atomic guarantees. It should only be used for debugging and writing
 * assertion statements. 
 */
bool IsSpinlockHeld(struct spinlock* lock) {
    return lock->lock;
}

int AcquireSpinlockIrql(struct spinlock* lock) {
    assert(lock->lock == 0);

    int prior_irql = GetIrql();
    RaiseIrql(lock->irql);

    if (lock->irql != GetIrql()) {
        Panic(PANIC_SPINLOCK_WRONG_IRQL);
    }

    AcquireSpinlockDirect(lock);
    lock->prev_irql = prior_irql;
    return prior_irql;
}

void ReleaseSpinlockIrql(struct spinlock* lock) {
    int old_irql = lock->prev_irql;
    ReleaseSpinlockDirect(lock);
    LowerIrql(old_irql);
}