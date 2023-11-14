#include <spinlock.h>
#include <string.h>
#include <arch.h>
#include <assert.h>

void InitSpinlock(struct spinlock* lock, const char* name) {
    assert(strlen(name) <= 15);

    lock->lock = 0;
    lock->owner = NULL;
    strcpy(lock->name, name);
}

void AcquireSpinlock(struct spinlock* lock) {
    (void) lock;

    assert(lock->lock == 0);

    ArchIrqSpinlockAcquire(&lock->lock);
    lock->owner = NULL; //GetCurrentThread();
}

bool TryAcquireSpinlock(struct spinlock* lock) {
    (void) lock;
    return false; 
}

bool RecursiveAcquireSpinlock(struct spinlock* lock) {
    // TODO: this is not atomic. may cause issues with both allocation AND free.

    (void) lock;
    return false;
}

void ReleaseSpinlock(struct spinlock* lock) {
    lock->owner = NULL;
    ArchIrqSpinlockRelease(&lock->lock);
}