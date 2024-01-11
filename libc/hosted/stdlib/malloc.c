#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <voidptr.h>

/*
 * See kernel/heap.c for a commented heap implementation (this is a simplified)
 * version of the kernel heap).
 */

struct block {
    size_t size;
    struct block* next;
    struct block* prev;
};

#define ALIGNMENT 8

#define METADATA_LEADING (sizeof(size_t))
#define METADATA_TRIALING (sizeof(size_t))
#define METADATA_TOTAL (METADATA_LEADING + METADATA_TRIALING)

#define MINIMUM_REQUEST_SIZE_INTERNAL (2 * sizeof(size_t))
#define TOTAL_NUM_FREE_LISTS 77

static size_t free_list_block_sizes[TOTAL_NUM_FREE_LISTS] = {
    8,          16,         24,         32,
    40,         48,         56,         64,
    72,         80,         88,         96,
    104,        112,        120,        128,
    160,        192,        224,        256,                
    320,        384,        448,        512,
    768,        1024,       1536,       2048,
    1024 * 3,   1024 * 4,   1024 * 5,   1024 * 6,  
    1024 * 7,   1024 * 8,   1024 * 9,   1024 * 10,
    1024 * 11,  1024 * 12,  1024 * 14,  1024 * 16,
    1024 * 20,  1024 * 24,  1024 * 28,  1024 * 32,
    1024 * 40,  1024 * 48,  1024 * 56,  1024 * 64,
    1024 * 80,  1024 * 96,  1024 * 112, 1024 * 128,
    1024 * 192, 1024 * 256, 1024 * 512, 1024 * 768,
    1024 * 1024 * 1,        1024 * 1024 * 3 / 2,
    1024 * 1024 * 2,        1024 * 1024 * 5 / 2,
    1024 * 1024 * 3,        1024 * 1024 * 7 / 2,
    1024 * 1024 * 4,        1024 * 1024 * 6,
    1024 * 1024 * 8,        1024 * 1024 * 12,
    1024 * 1024 * 16,       1024 * 1024 * 24,
    1024 * 1024 * 32,       1024 * 1024 * 40,
    1024 * 1024 * 48,       1024 * 1024 * 64,
    1024 * 1024 * 96,       1024 * 1024 * 128,
    1024 * 1024 * 256,      1024 * 1024 * 512,
    1024 * 1024 * 1024,
};

static int GetSmallestListIndexThatFits(size_t size_without_metadata) {
    int i = 0;
    while (true) {
        if (size_without_metadata <= free_list_block_sizes[i]) {
            return i;
        }
        ++i;
    }
}

static int GetInsertionIndex(size_t size_without_metadata) {
    assert(size_without_metadata >= free_list_block_sizes[0]);

    if (size_without_metadata == free_list_block_sizes[0]) {
        return 0;
    }
  
    for (int i = 0; i < TOTAL_NUM_FREE_LISTS - 1; ++i) {
        if (size_without_metadata <= free_list_block_sizes[i]) {
            return i - 1;
        }
    }

    assert(false);
    return 0;
}

static size_t RoundUpSize(size_t size) {
    assert(size != 0);

    if (size < MINIMUM_REQUEST_SIZE_INTERNAL) {
        size = MINIMUM_REQUEST_SIZE_INTERNAL;
    }

    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static void MarkFree(struct block* block) {
    block->size &= ~1;
}

static void MarkAllocated(struct block* block) {
    block->size |= 1;
}

static bool IsAllocated(struct block* block) {
    return block->size & 1;
}

static struct block* _head_block[TOTAL_NUM_FREE_LISTS];

static size_t GetSize(struct block* block) {
    size_t size = block->size & ~3;
    assert(*(((size_t*) block) + (size / sizeof(size_t)) - 1) == size);
    return size;
}

static void SetSizeTags(struct block* block, size_t size) {
    block->size = (block->size & 3) | size;
    *(((size_t*) block) + (size / sizeof(size_t)) - 1) = size;
}

static struct block* RequestBlock(size_t total_size) {
    total_size += MINIMUM_REQUEST_SIZE_INTERNAL * 2;

    struct block* block = mmap(0, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == NULL) {
        return NULL;
    }

    struct block* left_fence = block;
    struct block* actual_block = (struct block*) (((size_t*) block) + MINIMUM_REQUEST_SIZE_INTERNAL / sizeof(size_t));
    struct block* right_fence  = (struct block*) (((size_t*) block) + (total_size - MINIMUM_REQUEST_SIZE_INTERNAL) / sizeof(size_t));

    SetSizeTags(left_fence, MINIMUM_REQUEST_SIZE_INTERNAL);
    SetSizeTags(actual_block, total_size - 2 * MINIMUM_REQUEST_SIZE_INTERNAL);
    SetSizeTags(right_fence, MINIMUM_REQUEST_SIZE_INTERNAL);

    actual_block->prev = NULL;
    actual_block->next = NULL;
    
    MarkAllocated(left_fence);
    MarkAllocated(right_fence);
    MarkFree(actual_block);

    return actual_block;
}

static void RemoveBlock(int free_list_index, struct block* block) {
    struct block** head_list = _head_block;

    if (block->prev == NULL && block->next == NULL) {
        assert(head_list[free_list_index] == block);
        head_list[free_list_index] = NULL;
        
    } else if (block->prev == NULL) {
        head_list[free_list_index] = block->next;
        block->next->prev = NULL;

    } else if (block->next == NULL) {
        block->prev->next = NULL;

    } else {
        block->prev->next = block->next;
        block->next->prev = block->prev;
    }
}

static struct block* AddBlock(struct block* block) {
    size_t size = GetSize(block);
    struct block** head_list = _head_block;

    int free_list_index = GetInsertionIndex(size - METADATA_TOTAL);

    size_t prev_block_size = *(((size_t*) block) - 1);
    struct block* prev_block = (struct block*) (((size_t*) block) - prev_block_size / sizeof(size_t));
    struct block* next_block = (struct block*) (((size_t*) block) + size / sizeof(size_t));

    if (IsAllocated(prev_block) && IsAllocated(next_block)) {
        block->prev = NULL;
        block->next = head_list[free_list_index];
        if (block->next != NULL) {
            block->next->prev = block;
        }
        head_list[free_list_index] = block;
        MarkFree(block);
        return block;

    } else if (IsAllocated(prev_block) && !IsAllocated(next_block)) {
        RemoveBlock(GetInsertionIndex(GetSize(next_block) - METADATA_TOTAL), next_block);
        SetSizeTags(block, size + GetSize(next_block));
        block->prev = NULL;
        block->next = NULL;
        MarkFree(block);    
        return AddBlock(block);
    
    } else if (!IsAllocated(prev_block) && IsAllocated(next_block)) {
        RemoveBlock(GetInsertionIndex(GetSize(prev_block) - METADATA_TOTAL), prev_block);
        SetSizeTags(prev_block, size + GetSize(prev_block));
        prev_block->prev = NULL;
        prev_block->next = NULL;
        MarkFree(prev_block);
        return AddBlock(prev_block);

    } else {
        RemoveBlock(GetInsertionIndex(GetSize(prev_block) - METADATA_TOTAL), prev_block);
        RemoveBlock(GetInsertionIndex(GetSize(next_block) - METADATA_TOTAL), next_block);
        SetSizeTags(prev_block, size + GetSize(prev_block) + GetSize(next_block));
        prev_block->prev = NULL;
        prev_block->next = NULL;
        MarkFree(prev_block);
        return AddBlock(prev_block);
    }
}

static struct block* AllocateBlock(struct block* block, int free_list_index, size_t user_requested_size) {
    assert(block != NULL);

    size_t total_size = user_requested_size + METADATA_TOTAL;
    size_t block_size = GetSize(block);

    assert(block_size >= total_size);

    if (block_size - total_size < MINIMUM_REQUEST_SIZE_INTERNAL + METADATA_TOTAL) {
        RemoveBlock(free_list_index, block);
        SetSizeTags(block, block_size);
        MarkAllocated(block);
        return block;

    } else {
        RemoveBlock(free_list_index, block);

        size_t leftover = block_size - total_size;
        SetSizeTags(block, leftover);

        struct block* allocated_block = (struct block*) (((size_t*) block) + (leftover / sizeof(size_t)));
        SetSizeTags(allocated_block, total_size);
        MarkAllocated(allocated_block);
        MarkFree(block);
        AddBlock(block);
        return allocated_block;
    }
}

static struct block* FindBlock(size_t user_requested_size) {
    struct block** head_list = _head_block;

    int min_index = GetSmallestListIndexThatFits(user_requested_size);
    for (int i = min_index; i < TOTAL_NUM_FREE_LISTS; ++i) {
        if (head_list[i] != NULL) {
            return AllocateBlock(head_list[i], i, user_requested_size);
        }
    }

    size_t total_size = free_list_block_sizes[min_index + 1] + METADATA_TOTAL;
    struct block* sys_block = RequestBlock(total_size);
    int sys_index = GetInsertionIndex(GetSize(sys_block) - METADATA_TOTAL);
    assert(head_list[sys_index] == NULL);
    head_list[sys_index] = sys_block;
    return AllocateBlock(head_list[sys_index], sys_index, user_requested_size);
}

void* malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size = RoundUpSize(size);

    // TODO: lock
    struct block* block = FindBlock(size);
    // TODO: unlock

    assert(((size_t) block & (ALIGNMENT - 1)) == 0);

    return AddVoidPtr(block, METADATA_LEADING);
}

void free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    struct block* block = SubVoidPtr(ptr, METADATA_LEADING);
    block->prev = NULL;
    block->next = NULL;

    // TODO: lock...

    AddBlock(block);

    // TODO: unlock...
}

void* calloc(size_t num, size_t size) {
    // TODO: check for overflow on multiply
    void* mem = malloc(num * size);
    if (mem != NULL) {
        memset(mem, 0, num * size);
    }
    return mem;
}