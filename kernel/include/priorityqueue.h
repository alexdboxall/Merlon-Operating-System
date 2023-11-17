#pragma once

#include <stdbool.h>

struct priority_queue;
struct priority_queue_result;

struct priority_queue* PriorityQueueCreate(int capacity, bool max, int element_width);
void PriorityQueueInsert(struct priority_queue* queue, void* elem, int priority);
struct priority_queue_result PriorityQueuePeek(struct priority_queue* queue);
struct priority_queue_result PriorityQueuePop(struct priority_queue* queue);
int PriorityQueueGetCapacity(struct priority_queue* queue);
int PriorityQueueGetUsedSize(struct priority_queue* queue);