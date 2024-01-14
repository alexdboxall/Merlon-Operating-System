#pragma once

#include <common.h>

struct thread;

struct spinlock {
    size_t lock;
    char name[16];
    int irql;
    int prev_irql;
};

void InitSpinlock(struct spinlock* lock, const char* name, int irql);
int AcquireSpinlock(struct spinlock* lock);
void ReleaseSpinlock(struct spinlock* lock);
bool IsSpinlockHeld(struct spinlock* lock);