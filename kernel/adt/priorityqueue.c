

#include <heap.h>
#include <string.h>
#include <log.h>
#include <panic.h>
#include <assert.h>
#include <priorityqueue.h>

/*
 * Implements the max-heap and min-heap data structures. To avoid confusion with the
 * heap memory manager, it is referred to as a priority queue.
 */

struct priority_queue {
    int capacity;
    int size;
    int element_width;
    int qwords_per_element;       // includes the + 1 for the priority
    bool max;
    uint64_t* array;     // length is: capacity * qwords_per_element
};

struct priority_queue* PriorityQueueCreate(int capacity, bool max, int element_width) {
    assert(capacity > 0);
    assert(element_width > 0);

    struct priority_queue* queue = AllocHeap(sizeof(struct priority_queue));
    queue->capacity = capacity;
    queue->size = 0;
    queue->element_width = element_width;
    queue->qwords_per_element = 1 + (element_width + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    queue->max = max;
    queue->array = AllocHeap(sizeof(uint64_t) * queue->qwords_per_element * capacity);
    return queue;
}

void PriorityQueueDestroy(struct priority_queue* queue) {
    FreeHeap(queue->array);
    FreeHeap(queue);
}

static void SwapElements(struct priority_queue* queue, int a, int b) {
    a *= queue->qwords_per_element;
    b *= queue->qwords_per_element;

    for (int i = 0; i < queue->qwords_per_element; ++i) {
        uint64_t tmp = queue->array[a];
        queue->array[a] = queue->array[b];
        queue->array[b] = tmp;
        ++a; ++b;
    }
}

static void Heapify(struct priority_queue* queue, int i) {
    int extreme = i;
    int left = i * 2 + 1;
    int right = left + 1;

    if (left < queue->size) {
        if ((queue->max && queue->array[left * queue->qwords_per_element] > queue->array[extreme * queue->qwords_per_element]) || (!queue->max && queue->array[left * queue->qwords_per_element] < queue->array[extreme * queue->qwords_per_element])) {
            extreme = left;
        }
    }
    if (right < queue->size) {
        if ((queue->max && queue->array[right * queue->qwords_per_element] > queue->array[extreme * queue->qwords_per_element]) || (!queue->max && queue->array[right * queue->qwords_per_element] < queue->array[extreme * queue->qwords_per_element])) {
            extreme = right;
        }
    }
    if (i != extreme) {
        SwapElements(queue, i, extreme);
        Heapify(queue, extreme);
    }
}

void PriorityQueueInsert(struct priority_queue* queue, void* elem, uint64_t priority) {
    if (queue->size == queue->capacity) {
        PanicEx(PANIC_PRIORITY_QUEUE, "insert called when full");
    }

    int i = queue->size++;
    queue->array[i * queue->qwords_per_element] = priority;
    memcpy(queue->array + i * queue->qwords_per_element + 1, elem, queue->element_width);

    if (queue->max) {
        while (i != 0 && queue->array[((i - 1) / 2) * queue->qwords_per_element] < queue->array[i * queue->qwords_per_element]) {
            SwapElements(queue, (i - 1) / 2, i);
            i = (i - 1) / 2;
        }
    } else {
        while (i != 0 && queue->array[((i - 1) / 2) * queue->qwords_per_element] > queue->array[i * queue->qwords_per_element]) {
            SwapElements(queue, (i - 1) / 2, i);
            i = (i - 1) / 2;
        }
    }
}

/*
 * returns the priority, and a REFERENCE to the data IN THE PRIORITY QUEUE. It is NOT a copy!
 * This is why Pop() can't return it, because popping it overwrites the data!
 */
struct priority_queue_result PriorityQueuePeek(struct priority_queue* queue) {
    if (queue->size == 0) {
        PanicEx(PANIC_PRIORITY_QUEUE, "peek called on empty");
    }

    struct priority_queue_result retv;
    retv.priority = queue->array[0];
    retv.data = (void*) (queue->array + 1);
    return retv;
}

/*
 * doesn't return the value, as the value would have been erased in the pop (PriorityQueue doesn't
 * allocate memory, as it needs to be used in high IRQL situations).
 */
void PriorityQueuePop(struct priority_queue* queue) {
    if (queue->size == 0) {
        PanicEx(PANIC_PRIORITY_QUEUE, "pop called on empty");
    }

    --queue->size;
    for (int i = 0; i < queue->qwords_per_element; ++i) {
        queue->array[i] = queue->array[queue->size * queue->qwords_per_element + i];
    }

    Heapify(queue, 0);
}

int PriorityQueueGetCapacity(struct priority_queue* queue) {
    return queue->capacity;
}

int PriorityQueueGetUsedSize(struct priority_queue* queue) {
    return queue->size;
}