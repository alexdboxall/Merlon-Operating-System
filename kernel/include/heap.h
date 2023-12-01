#pragma once

#include <common.h>

/*
 * Allocation will not fault. If this is impossible to achieve, the system will panic.
 * Must not be set with HEAP_ALLOW_PAGING.
 */
#define HEAP_NO_FAULT       1

/*
 * Clears allocated memory to zero. 
 */
#define HEAP_ZERO           2

/*
 * Indicates that the allocated region is allowed to be swapped onto disk.
 * Must not be set with HEAP_NO_FAULT. Data allocated with this flag set can only be
 * accessed when IRQL = IRQL_STANDARD.
 *
 * HEAP_ALLOW_PAGING makes a best effort to put in on the swappable heap. If HEAP_FORCE_PAGING is set too/instead,
 * then it will allocate a new block if it is unable to get an existing swappable memory.
 */
#define HEAP_ALLOW_PAGING   4   
#define HEAP_FORCE_PAGING   8

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