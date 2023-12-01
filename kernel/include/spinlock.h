#pragma once

#include <common.h>

struct thread;

struct spinlock {
    size_t lock;
    struct thread* owner;
    char name[16];
    int irql;
    int prev_irql;
};

void InitSpinlock(struct spinlock* lock, const char* name, int irql);

int AcquireSpinlockIrql(struct spinlock* lock);
void ReleaseSpinlockIrql(struct spinlock* lock);

void AcquireSpinlockDirect(struct spinlock* lock);
void ReleaseSpinlockDirect(struct spinlock* lock);

bool IsSpinlockHeld(struct spinlock* lock);