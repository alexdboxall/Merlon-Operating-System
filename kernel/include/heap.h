#pragma once

#include <common.h>

void* AllocHeap(size_t size);
void* ReallocHeap(void* ptr, size_t size);
void* AllocHeapZero(size_t size);
void FreeHeap(void* ptr);
void InitHeap(void);

#define malloc(x) AllocHeap(x)
#define free(x) FreeHeap(x)