#pragma once

#include <common.h>

struct thread;

struct spinlock {
    size_t lock;
    struct thread* owner;
    char name[16];
    int irql; 
};

void InitSpinlock(struct spinlock* lock, const char* name, int irql);
int AcquireSpinlock(struct spinlock* lock, bool raise_irql);
void ReleaseSpinlock(struct spinlock* lock);
void ReleaseSpinlockAndLower(struct spinlock* lock, int new_irql);
bool IsSpinlockHeld(struct spinlock* lock);