
#include <heap.h>
#include <string.h>
#include <log.h>
#include <panic.h>
#include <assert.h>
#include <common.h>
#include <priorityqueue.h>

/*
 * Implements the max-heap and min-heap data structures.
 */

struct heap_adt {
    int capacity;
    int size;
    int element_width;
    int qwords_per_elem;       // includes the + 1 for the priority
    bool max;
    uint64_t* array;           // length is: capacity * qwords_per_elem
};

struct heap_adt* HeapAdtCreate(int capacity, bool max, int element_width) {
    assert(capacity > 0);
    assert(element_width > 0);

    struct heap_adt* heap = AllocHeap(sizeof(struct heap_adt));
    heap->capacity = capacity;
    heap->size = 0;
    heap->element_width = element_width;
    heap->qwords_per_elem = 1 + (element_width + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    heap->max = max;
    heap->array = AllocHeap(sizeof(uint64_t) * heap->qwords_per_elem * capacity);
    return heap;
}

void HeapAdtDestroy(struct heap_adt* heap) {
    FreeHeap(heap->array);
    FreeHeap(heap);
}

static void SwapElements(struct heap_adt* heap, int a, int b) {
    a *= heap->qwords_per_elem;
    b *= heap->qwords_per_elem;

    for (int i = 0; i < heap->qwords_per_elem; ++i) {
        uint64_t tmp = heap->array[a];
        heap->array[a] = heap->array[b];
        heap->array[b] = tmp;
        ++a; ++b;
    }
}

static int GetMinOrMaxIndex(struct heap_adt* heap, int a, int b) {
    if (a >= heap->size || b >= heap->size) {
        /* 
         * The smaller index will always be in bounds, as we always call this 
         * with at least one good index (either `i` or a previous return value).
         */
        return MIN(a, b);
    }
    size_t qwords = heap->qwords_per_elem;
    bool is_a_small = heap->array[a * qwords] < heap->array[b * qwords];
    return (heap->max ^ is_a_small) ? a : b;
}

static void Heapify(struct heap_adt* heap, int i) {
    int left = i * 2 + 1;
    int right = left + 1;
    int best = GetMinOrMaxIndex(heap, right, GetMinOrMaxIndex(heap, i, left));
    if (i != best) {
        SwapElements(heap, i, best);
        Heapify(heap, best);
    }
}

void HeapAdtInsert(struct heap_adt* heap, void* elem, uint64_t priority) {
    if (heap->size == heap->capacity) {
        PanicEx(PANIC_PRIORITY_QUEUE, "insert called when full");
    }

    size_t qwords = heap->qwords_per_elem;
    uint64_t* arr = heap->array;

    int i = heap->size++;
    arr[i * qwords] = priority;
    inline_memcpy(arr + i * qwords + 1, elem, heap->element_width);

    while (i && (heap->max ^ (arr[((i - 1) / 2) * qwords] > arr[i * qwords]))) {
        SwapElements(heap, (i - 1) / 2, i);
        i = (i - 1) / 2;
    }
}

/*
 * Returns the priority, and a *reference* to the data in the priority queue.
 * It is not a copy!!
 */
struct heap_adt_result HeapAdtPeek(struct heap_adt* heap) {
    if (heap->size == 0) {
        PanicEx(PANIC_PRIORITY_QUEUE, "peek called on empty");
    }

    return (struct heap_adt_result) {
        .data = (void*) (heap->array + 1), .priority = heap->array[0]
    };
}

/*
 * Doesn't return the value, as the value would have been erased in the pop 
 * (HeapAdt doesn't allocate memory due to its need for use in high IRQLs).
 */
void HeapAdtPop(struct heap_adt* heap) {
    if (heap->size == 0) {
        PanicEx(PANIC_PRIORITY_QUEUE, "pop called on empty");
    }

    --heap->size;
    for (int i = 0; i < heap->qwords_per_elem; ++i) {
        heap->array[i] = heap->array[heap->size * heap->qwords_per_elem + i];
    }

    Heapify(heap, 0);
}

int HeapAdtGetCapacity(struct heap_adt* heap) {
    return heap->capacity;
}

int HeapAdtGetUsedSize(struct heap_adt* heap) {
    return heap->size;
}
