#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef COMPILE_KERNEL
#include <heap.h>
#include <log.h>
#else
#include <stdlib.h>
#endif

// TODO: heap sort is probably better for kernel use due to it not requiring
// additional memory. you do already have the heapify function in adt/priorityqueue.c
// that could potentially be useful!

static void Merge(void* array, size_t low, size_t mid, size_t high, size_t size, int (*compar)(const void *, const void *), bool allow_paging) {
    size_t count_1 = mid - low + 1;
    size_t count_2 = high - mid;

    (void) allow_paging;

#ifdef COMPILE_KERNEL
    void* temp_1 = allow_paging ? AllocHeapEx(count_1 * size, HEAP_ALLOW_PAGING) : AllocHeap(count_1 * size);
    void* temp_2 = allow_paging ? AllocHeapEx(count_2 * size, HEAP_ALLOW_PAGING) : AllocHeap(count_2 * size);
#else
    void* temp_1 = malloc(count_1 * size);
    void* temp_2 = malloc(count_2 * size);
#endif

    memcpy(temp_1, (const void*) (((uint8_t*) array) + low * size), count_1 * size);
    memcpy(temp_2, (const void*) (((uint8_t*) array) + (mid + 1) * size), count_2 * size);

    size_t index_1 = 0;
    size_t index_2 = 0;
    size_t out_index = low;
    while (index_1 < count_1 && index_2 < count_2) {
        const void* item_1 = (const void*) (((uint8_t*) temp_1) + size * index_1);
        const void* item_2 = (const void*) (((uint8_t*) temp_2) + size * index_2);

        if (compar(item_1, item_2) <= 0) {
            memcpy((void*) (((uint8_t*) array) + out_index * size), item_1, size);
            ++index_1;

        } else {
            memcpy((void*) (((uint8_t*) array) + out_index * size), item_2, size);
            ++index_2;
        }

        ++out_index;
    }

    while (index_1 < count_1) {
        const void* item_1 = (const void*) (((uint8_t*) temp_1) + size * index_1);
        memcpy((void*) (((uint8_t*) array) + out_index * size), item_1, size);
        ++index_1;
        ++out_index;
    }

    while (index_2 < count_2) {
        const void* item_2 = (const void*) (((uint8_t*) temp_2) + size * index_2);
        memcpy((void*) (((uint8_t*) array) + out_index * size), item_2, size);
        ++index_2;
        ++out_index;
    }

    free(temp_1);
    free(temp_2);
}

static void MergeSort(void* array, size_t low, size_t high, size_t size, int (*compar)(const void *, const void *), bool allow_paging) {
    if (low < high) {
        size_t mid = low + (high - low) / 2;
        MergeSort(array, low, mid, size, compar, allow_paging);
        MergeSort(array, mid + 1, high, size, compar, allow_paging);
        Merge(array, low, mid, high, size, compar, allow_paging);
    }
}

void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    MergeSort(base, 0, nmemb - 1, size, compar, false);
}

#ifdef COMPILE_KERNEL
void qsort_pageable(void* base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    MergeSort(base, 0, nmemb - 1, size, compar, true);
}
#endif

void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    size_t low = 0;
    size_t high = nmemb - 1;

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        const void* mid_item = (const void*) (((uint8_t*) base) + size * mid);
        int compare_result = compar(key, mid_item);

        if (compare_result == 0) {
            return (void*) mid_item;

        } else if (compare_result < 0) {
            high = mid - 1;

        } else {
            low = mid + 1;
        }
    }

    return NULL;
}
