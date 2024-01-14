#pragma once

#include <stdbool.h>

struct heap_adt;

struct heap_adt_result {
    uint64_t priority;
    void* data;
};

struct heap_adt* HeapAdtCreate(int capacity, bool max, int element_width);
void HeapAdtInsert(struct heap_adt* queue, void* elem, uint64_t priority);
struct heap_adt_result HeapAdtPeek(struct heap_adt* queue);
void HeapAdtPop(struct heap_adt* queue);
int HeapAdtGetCapacity(struct heap_adt* queue);
int HeapAdtGetUsedSize(struct heap_adt* queue);
void HeapAdtDestroy(struct heap_adt* queue);