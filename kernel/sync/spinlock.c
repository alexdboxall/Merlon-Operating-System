#include <spinlock.h>
#include <string.h>
#include <arch.h>
#include <irql.h>
#include <panic.h>
#include <log.h>
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

int AcquireSpinlock(struct spinlock* lock, bool raise_irql) {
    assert(lock->lock == 0);

    if (raise_irql) {
        RaiseIrql(lock->irql);

    } else if (lock->irql != GetIrql()) {
        Panic(PANIC_SPINLOCK_WRONG_IRQL);
    }

    ArchIrqSpinlockAcquire(&lock->lock);
    //lock->owner = GetCurrentThread();
    return lock->irql;
}

void ReleaseSpinlock(struct spinlock* lock) {
    lock->owner = NULL;
    ArchIrqSpinlockRelease(&lock->lock);
}

void ReleaseSpinlockAndLower(struct spinlock* lock, int new_irql) {
    ReleaseSpinlock(lock);
    LowerIrql(new_irql);
}