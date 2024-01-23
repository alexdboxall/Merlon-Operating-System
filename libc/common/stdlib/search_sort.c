#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef COMPILE_KERNEL
#include <heap.h>
#include <irql.h>
#include <assert.h>
#include <virtual.h>
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

    int size_1 = count_1 * size;
    int size_2 = count_2 * size;

#ifdef COMPILE_KERNEL
    void* temp_1 = size_1 < 1024 ? AllocHeap(size_1) : MapVirtEasy(size_1, allow_paging);
    void* temp_2 = size_2 < 1024 ? AllocHeap(size_2) : MapVirtEasy(size_2, allow_paging);
#else
    (void) allow_paging;
    void* temp_1 = malloc(size_1);
    void* temp_2 = malloc(size_2);
#endif

    memcpy(temp_1, (const void*) (((uint8_t*) array) + low * size), size_1);
    memcpy(temp_2, (const void*) (((uint8_t*) array) + (mid + 1) * size), size_2);

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

#ifdef COMPILE_KERNEL
    if (size_1 < 1024) {
        FreeHeap(temp_1);
    } else {
        UnmapVirt((size_t) temp_1, size_1);
    }
    if (size_2 < 1024) {
        FreeHeap(temp_2);
    } else {
        UnmapVirt((size_t) temp_2, size_2);
    }
#else
    free(temp_1);
    free(temp_2);
#endif
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
