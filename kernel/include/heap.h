#pragma once

#include <common.h>

#define HEAP_ZERO           1

void* AllocHeap(size_t size);
void* AllocHeapEx(size_t size, int flags);
void* ReallocHeap(void* ptr, size_t size);
void* AllocHeapZero(size_t size);
void FreeHeap(void* ptr);
void InitHeap(void);

#ifndef NDEBUG
int DbgGetOutstandingHeapAllocations(void);
#endif

#define malloc(x) AllocHeap(x)
#define free(x) FreeHeap(x)