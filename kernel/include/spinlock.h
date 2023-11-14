#pragma once

#include <common.h>

struct spinlock {
    size_t lock;
    void* owner;
    char name[16]; 
};

void InitSpinlock(struct spinlock* lock, const char* name);
void AcquireSpinlock(struct spinlock* lock);
bool TryAcquireSpinlock(struct spinlock* lock);
bool RecursiveAcquireSpinlock(struct spinlock* lock);
void ReleaseSpinlock(struct spinlock* lock);
