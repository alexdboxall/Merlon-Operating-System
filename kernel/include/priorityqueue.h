#pragma once

#include <stdbool.h>

struct priority_queue;

struct priority_queue_result {
    uint64_t priority;
    void* data;
};

export struct priority_queue* PriorityQueueCreate(int capacity, bool max, int element_width);
export void PriorityQueueInsert(struct priority_queue* queue, void* elem, uint64_t priority);
export struct priority_queue_result PriorityQueuePeek(struct priority_queue* queue);
export void PriorityQueuePop(struct priority_queue* queue);
export int PriorityQueueGetCapacity(struct priority_queue* queue);
export int PriorityQueueGetUsedSize(struct priority_queue* queue);
export void PriorityQueueDestroy(struct priority_queue* queue);