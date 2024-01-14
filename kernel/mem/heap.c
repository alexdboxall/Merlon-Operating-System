#include <common.h>
#include <assert.h>
#include <stdbool.h>
#include <panic.h>
#include <virtual.h>
#include <arch.h>
#include <debug.h>
#include <string.h>
#include <spinlock.h>
#include <heap.h>
#include <log.h>
#include <semaphore.h>
#include <voidptr.h>
#include <irql.h>
#include <thread.h>

#define BOOTSTRAP_SIZE (1024 * 16)
#define MAX_EMERGENCY_BLOCKS 16

/*
 * For non-pageable requests larger than this, we'll issue a warning to say that
 * MapVirt is a much better choice.
 */
#define WARNING_LARGE_REQUEST_SIZE (1024 * 3 + 512)

struct emergency_block {
    uint8_t* address;
    size_t size;
    bool valid;
};

/**
 * Used as a system block that can be used even before the virtual memory 
 * manager is setup.
 */
static uint8_t bootstrap_memory_area[BOOTSTRAP_SIZE] __attribute__ ((aligned(ARCH_PAGE_SIZE)));

/*
 * Used to give us memory when we are not allowed to fault (i.e. can't allocate
 * virtual memory).
 */
static struct emergency_block emergency_blocks[MAX_EMERGENCY_BLOCKS] = {
    {.address = bootstrap_memory_area, .size = BOOTSTRAP_SIZE, .valid = true}
};

static struct semaphore* heap_lock;
static struct spinlock heap_spinlock;
static struct spinlock heap_locker_lock;
static struct thread* lock_entry_threads[2];

static bool LockHeap(bool paging) {
    bool acquired = false;
    struct thread* thr = GetThread();

    AcquireSpinlock(&heap_locker_lock);
    if (lock_entry_threads[paging] != thr || lock_entry_threads[paging] == NULL) {
        ReleaseSpinlock(&heap_locker_lock);

        if (paging) {
            AcquireMutex(heap_lock, -1);
        } else {
            AcquireSpinlock(&heap_spinlock);
        }

        lock_entry_threads[paging] = thr;
        acquired = true;

    } else {
        ReleaseSpinlock(&heap_locker_lock);
    }

    return acquired;
}

static void UnlockHeap(bool paging) {
    lock_entry_threads[paging] = NULL;

    if (paging) {
        ReleaseMutex(heap_lock);

    } else {
        ReleaseSpinlock(&heap_spinlock);
    }   
}

static void* AllocateFromEmergencyBlocks(size_t size) {
    int smallest = -1;

    for (int i = 0; i < MAX_EMERGENCY_BLOCKS; ++i) {
        if (emergency_blocks[i].valid && emergency_blocks[i].size >= size) {
            if (smallest == -1 || emergency_blocks[i].size < emergency_blocks[smallest].size) {
                smallest = i;
            }
        }
    }

    if (smallest == -1) {
        Panic(PANIC_CANNOT_MALLOC_WITHOUT_FAULTING);
    }

    void* address = emergency_blocks[smallest].address;

    emergency_blocks[smallest].address += size;
    emergency_blocks[smallest].size -= size;

    if (emergency_blocks[smallest].size < ARCH_PAGE_SIZE) {
        emergency_blocks[smallest].valid = false;
    }

    return address;
}

/*
 * This function needs to be called with the heap lock held.
 */
static void AddBlockToBackupHeap(size_t size) {
    UnlockHeap(false);
    void* address = (void*) MapVirt(0, 0, size, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
    LockHeap(false);

    int small_index = 0;
    for (int i = 0; i < MAX_EMERGENCY_BLOCKS; ++i) {
        if (emergency_blocks[i].valid) {
            if (emergency_blocks[i].size < emergency_blocks[small_index].size) {
                small_index = i;
            }
        } else {
            emergency_blocks[i].valid = true;
            emergency_blocks[i].size = size;
            emergency_blocks[i].address = address;
            return;
        }
    }

    LogDeveloperWarning(
        "losing 0x%X bytes due to strangeness with backup heap.\n", 
        emergency_blocks[small_index].size
    );

    emergency_blocks[small_index].size = size;
    emergency_blocks[small_index].address = address;
}

static void RestoreEmergencyPages(void*) {
    size_t total_size = 0;
    size_t largest_block = 0;
    
    LockHeap(false);
    for (int i = 0; i < MAX_EMERGENCY_BLOCKS; ++i) {
        if (emergency_blocks[i].valid) {
            size_t size = emergency_blocks[i].size;
            total_size += size;
            if (size > largest_block) {
                largest_block = size;
            }
        }
    }

    while (largest_block < BOOTSTRAP_SIZE / 2 || total_size < BOOTSTRAP_SIZE) {
        AddBlockToBackupHeap(BOOTSTRAP_SIZE);
        total_size += BOOTSTRAP_SIZE;
        largest_block = BOOTSTRAP_SIZE;
    }

    UnlockHeap(false);
}

/**
 * Represents a section of memory that is either allocated or free. The memory 
 * address it represents is itself, excluding the metadata at the start or end.
 */
struct block {
    /*
     * The entire size of the block. The low 2 bits do not form part of the 
     * size, the low bit is set for allocated blocks, and the second lowest bit 
     * is indicates if it is on the swappable heap or not.
     */
    size_t size;

    /*
     * Only here on free blocks. Allocated blocks use this as the start of 
     * allocated memory.
     */
    struct block* next;
    struct block* prev;

    /*
     * At position size - sizeof(size_t), there is the trailing size tag. There
     * are no flags in the low bit of this value, unlike the heading size tag.
     */
};

#ifndef NDEBUG
static int outstanding_allocations = 0;

int DbgGetOutstandingHeapAllocations(void) {
    return outstanding_allocations;
}
#endif

/**
 * Must be a power of 2.
 */
#define ALIGNMENT 8

/**
 * The amount of metadata at the start and end of allocated blocks. The next and
 * free pointers in free blocks do not count.
 */
#define METADATA_LEADING (sizeof(size_t))
#define METADATA_TRIALING (sizeof(size_t))
#define METADATA_TOTAL (METADATA_LEADING + METADATA_TRIALING)

#define MINIMUM_REQUEST_SIZE_INTERNAL (2 * sizeof(size_t))
#define TOTAL_NUM_FREE_LISTS 35

/**
 * An array which holds the minimum allocation sizes that each free list can 
 * hold.
 */
static size_t free_list_block_sizes[TOTAL_NUM_FREE_LISTS] = {
    8,          16,         24,         32,
    40,         48,         56,         64,
    72,         80,         88,         96,
    104,        112,        120,        128,
    160,        192,        224,        256,                
    320,        384,        448,        512,
    768,        1024,       1536,       2048,       // 28 [27]
    1024 * 4,   1024 * 8,   1024 * 16,  1024 * 32,  // 32 [31]
    1024 * 64,  1024 * 128, 1024 * 256,
};

/**
 * Used to work out which free list a block should be in, when we are *reading*
 * a block. This rounds the size *up*, meaning it cannot be used to insert a 
 * block into a list. NOT USED TO INSERT BLOCKS!! 
 */
static int GetSmallestListIndexThatFits(size_t size_without_metadata) {
    int i = 0;
    while (true) {
        if (size_without_metadata <= free_list_block_sizes[i]) {
            return i;
        }
        ++i;
    }
}

/**
 * Calculates which free list a block should be inserted in. This rounds down,
 * and so it should not normally be used to look up where a block should be.
 */
static int GetInsertionIndex(size_t size_without_metadata) {
    assert(size_without_metadata >= free_list_block_sizes[0]);

    /*
     * We can't round down to the next one when it's the smallest possible size,
     * so handle this case specially.
     */
    if (size_without_metadata == free_list_block_sizes[0]) {
        return 0;
    }
  
    for (int i = 0; i < TOTAL_NUM_FREE_LISTS - 1; ++i) {
        if (size_without_metadata <= free_list_block_sizes[i]) {
            return i - 1;
        }
    }

    Panic(PANIC_HEAP_REQUEST_TOO_LARGE);
}

/**
 * Rounds up a user-supplied allocation size to the alignment. If it's less than
 * the minimum size internally supported, we will round it up to that size.
 */
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

static void MarkSwappability(struct block* block, int can_swap) {
    if (can_swap) {
        block->size |= 2;
    } else {
        block->size &= ~2;
    }
}

static bool IsOnSwappableHeap(struct block* block) {
    return block->size & 2;
}

/**
 * Global arrays always initialise to zero (and therefore, to NULL).
 * Entries in free lists must have a user allocated size GREATER OR EQUAL TO the
 * size in free_list_block_sizes.
 */
static struct block* _head_block[TOTAL_NUM_FREE_LISTS];
static struct block* _head_block_swappable[TOTAL_NUM_FREE_LISTS];

static struct block** GetHeap(bool swappable) {
    return swappable ? _head_block_swappable : _head_block;
}

static struct block** GetHeapForBlock(struct block* block) {
    return GetHeap(IsOnSwappableHeap(block));
}


/**
 * Given a block, returns its total size, including metadata. This takes into 
 * account the flags on the size field and removes them from the return value. 
 */
static size_t GetSize(struct block* block) {
    size_t size = block->size & ~3;
    assert(*(((size_t*) block) + (size / sizeof(size_t)) - 1) == size);
    return size;
}

/**
 * Sets the *total* size of a given block. This does not do any checking, so the
 * caller must be careful as setting the wrong size can corrupt the heap.
 * Carries the swappability and allocation tags with it from the front to the 
 * back tags.
 */
static void SetSizeTags(struct block* block, size_t size) {
    block->size = (block->size & 3) | size;
    *(((size_t*) block) + (size / sizeof(size_t)) - 1) = size;
}

static void* GetSystemMemory(size_t size, int flags) {
    if (flags & HEAP_NO_FAULT) {
        if (flags & HEAP_ALLOW_PAGING) {
            Panic(PANIC_CONFLICTING_ALLOCATION_REQUIREMENTS);
        }
        if (IsVirtInitialised()) {
            DeferUntilIrql(IRQL_STANDARD, RestoreEmergencyPages, NULL);
        }
        return AllocateFromEmergencyBlocks(size);
    }

    return (void*) MapVirt(
        0,  0, size, 
        VM_READ | VM_WRITE | (flags & HEAP_ALLOW_PAGING ? 0 : VM_LOCK), 
        NULL, 0
    );
}

/**
 * Allocates a new block from the system that is able to hold the amount of data
 * specified. Also allocated enough memory for fenceposts on either side of the 
 * data, and sets up these fenceposts correctly.
 */
static struct block* RequestBlock(size_t total_size, int flags) {
    /*
     * We need to add the extra bytes for fenceposts to be added. We must do 
     * this before we round up to the nearest areana size (if we did it after, 
     * it wouldn't be aligned anymore).
     */
    total_size += MINIMUM_REQUEST_SIZE_INTERNAL * 2;
    total_size = (total_size + ARCH_PAGE_SIZE - 1) & ~(ARCH_PAGE_SIZE - 1);

    if (!IsVirtInitialised()) {
        flags |= HEAP_NO_FAULT;
    }

    struct block* block = (struct block*) GetSystemMemory(total_size, flags);
    if (block == NULL) {
        Panic(PANIC_OUT_OF_HEAP);
    }

    /*
     * Set the metadata for both the fenceposts and the main data block. 
     * Keep in mind that total_size now includes the fencepost metadata (see top
     * of function), so this sometimes needs to be subtracted off.
     */
    size_t actual_block_offset = MINIMUM_REQUEST_SIZE_INTERNAL / sizeof(size_t);
    size_t right_block_offset = (total_size - MINIMUM_REQUEST_SIZE_INTERNAL) / sizeof(size_t);

    struct block* left_fence = block;
    struct block* actual_block = (struct block*) (((size_t*) block) + actual_block_offset);
    struct block* right_fence  = (struct block*) (((size_t*) block) + right_block_offset);

    SetSizeTags(left_fence, MINIMUM_REQUEST_SIZE_INTERNAL);
    SetSizeTags(actual_block, total_size - 2 * MINIMUM_REQUEST_SIZE_INTERNAL);
    SetSizeTags(right_fence, MINIMUM_REQUEST_SIZE_INTERNAL);

    actual_block->prev = NULL;
    actual_block->next = NULL;
    
    MarkAllocated(left_fence);
    MarkAllocated(right_fence);
    MarkFree(actual_block);

    MarkSwappability(left_fence, flags & HEAP_ALLOW_PAGING);
    MarkSwappability(right_fence, flags & HEAP_ALLOW_PAGING);
    MarkSwappability(actual_block, flags & HEAP_ALLOW_PAGING);

    return actual_block;
}

/**
 * Removes a block from a free list. It needs to take in the exact free list's 
 * index (as opposed to calculating it itself), as this may be used halfway 
 * though allocations or deallocations where the block isn't yet in its correct 
 * block.
 */
static void RemoveBlock(int free_list_index, struct block* block) {
    struct block** head_list = GetHeapForBlock(block);

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

/**
 * Adds a block to its appropriate free list. It also coalesces the block with 
 * surrounding free blocks if possible.
 */
static struct block* AddBlock(struct block* block) {
    size_t size = GetSize(block);
    struct block** head_list = GetHeapForBlock(block);

    int free_list_index = GetInsertionIndex(size - METADATA_TOTAL);

    size_t prev_size = *(((size_t*) block) - 1);
    struct block* prev = (struct block*) (((size_t*) block) - prev_size / sizeof(size_t));
    struct block* next = (struct block*) (((size_t*) block) + size / sizeof(size_t));

    if (IsAllocated(prev) && IsAllocated(next)) {
        /*
         * Cannot coalesce here, so just add to the free list.
         */
        block->prev = NULL;
        block->next = head_list[free_list_index];
        if (block->next != NULL) {
            block->next->prev = block;
        }
        head_list[free_list_index] = block;
        MarkFree(block);
        return block;

    } else if (IsAllocated(prev) && !IsAllocated(next)) {
        /*
         * Need to coalesce with the one on the right.
         */
        bool swappable = IsOnSwappableHeap(block);
        assert(swappable == IsOnSwappableHeap(next));
    
        RemoveBlock(GetInsertionIndex(GetSize(next) - METADATA_TOTAL), next);
        SetSizeTags(block, size + GetSize(next));
        block->prev = NULL;
        block->next = NULL;
        MarkFree(block);
        MarkSwappability(block, swappable);
    
        return AddBlock(block);
    
    } else if (!IsAllocated(prev) && IsAllocated(next)) {
        /*
         * Need to coalesce with the one on the left.
         */
        bool swappable = IsOnSwappableHeap(block);
        assert(swappable == IsOnSwappableHeap(prev));

        RemoveBlock(GetInsertionIndex(GetSize(prev) - METADATA_TOTAL), prev);
        SetSizeTags(prev, size + GetSize(prev));
        prev->prev = NULL;
        prev->next = NULL;
        MarkFree(prev);
        MarkSwappability(prev, swappable);

        return AddBlock(prev);

    } else {
        /*
         * Coalesce with blocks on both sides.
         */
        bool swappable = IsOnSwappableHeap(block);
        assert(swappable == IsOnSwappableHeap(prev));
        assert(swappable == IsOnSwappableHeap(next));

        RemoveBlock(GetInsertionIndex(GetSize(prev) - METADATA_TOTAL), prev);
        RemoveBlock(GetInsertionIndex(GetSize(next) - METADATA_TOTAL), next);

        SetSizeTags(prev, size + GetSize(prev) + GetSize(next));
        prev->prev = NULL;
        prev->next = NULL;
        MarkFree(prev);
        MarkSwappability(prev, swappable);

        return AddBlock(prev);
    }
}

/*
 * Allocates a block. The block to be allocated will be the first block in the 
 * given free list, and that free list must be non-empty, and be able to fit the
 * requested size.
 */
static struct block* AllocateBlock(
    struct block* block, 
    int free_list_index, 
    size_t user_requested_size
) {
    assert(block != NULL);

    size_t total_size = user_requested_size + METADATA_TOTAL;
    size_t block_size = GetSize(block);

    assert(block_size >= total_size);

    if (block_size - total_size < MINIMUM_REQUEST_SIZE_INTERNAL + METADATA_TOTAL) {
        /*
         * We can just remove from the list altogether if the sizes match up 
         * exactly, or if there would be so little left over that we can't form 
         * a new block.
         */
        RemoveBlock(free_list_index, block);
        /*
         * Prevent memory leak (from having a hole in memory), but do it after 
         * removing the block, as this may change the list it needs to be in, 
         * and RemoveBlock will not like that.
         */
        SetSizeTags(block, block_size);
        MarkAllocated(block);
        return block;

    } else {
        /*
         * We must split the block into two. If no list change is needed, we can
         * leave the 'leftover' parts in the list as is (just fixing up the size
         * tags), and then return the new block.
         */

        RemoveBlock(free_list_index, block);

        size_t leftover = block_size - total_size;
        SetSizeTags(block, leftover);

        size_t offset = leftover / sizeof(size_t);
        struct block* allocated_block = (struct block*) (((size_t*) block) + offset);
        SetSizeTags(allocated_block, total_size);

        /*
         * We are giving it new tags, so must set this correctly.
         */
        MarkSwappability(allocated_block, IsOnSwappableHeap(block));

        /*
         * Must be done before we try to move around the leftovers (or else it 
         * will coalesce back into one block). 
         */
        MarkAllocated(allocated_block);

        /*
        * We need to remove the leftover block from this list, and add it to the
        * correct list.
        */
        MarkFree(block);
        AddBlock(block);
        return allocated_block;
    }
}

/**
 * Allocates a block that can fit the user requested size. It will request new 
 * memory from the system if required. If it returns NULL, then there is not 
 * enough memory of the system to satisfy the request.
 */
static struct block* FindBlock(size_t user_requested_size, int flags) {
    struct block** head_list = GetHeap(flags & HEAP_ALLOW_PAGING);

    int min_index = GetSmallestListIndexThatFits(user_requested_size);
    for (int i = min_index; i < TOTAL_NUM_FREE_LISTS; ++i) {
        if (head_list[i] != NULL) {
            return AllocateBlock(head_list[i], i, user_requested_size);
        }
    }

    /*
     * If we can't find a block that will fit, then we must allocate memory.
     * Round up to the next block size, so we ensure we are in the next bucket.
     * This avoids an issue if e.g. a user requests 2.1KB, and we allocate 3.9KB
     * and it goes in the wrong bucket due to the two different indexes used.
     */
    size_t total_size = free_list_block_sizes[min_index + 1] + METADATA_TOTAL;
    struct block* sys_block = RequestBlock(total_size, flags);

    /*  
     * Put the new memory in the free list (which ought to be empty, as wouldn't
     * need to request new memory otherwise). Then we can allocate the block.
     */
    int sys_index = GetInsertionIndex(GetSize(sys_block) - METADATA_TOTAL);
    assert(head_list[sys_index] == NULL);
    head_list[sys_index] = sys_block;
    return AllocateBlock(head_list[sys_index], sys_index, user_requested_size);
}

/**
 * Allocates memory on the heap. Unless you *really* know what you're doing, you
 * should always pass HEAP_NO_FAULT. AllocHeap passes this automatically, but 
 * this one doesn't (in case you want to allocate from the pagable pool).
 */
void* AllocHeapEx(size_t size, int flags) {
    MAX_IRQL(IRQL_SCHEDULER);
    if (flags & HEAP_ALLOW_PAGING) {
        EXACT_IRQL(IRQL_STANDARD);
    }

    if (size == 0) {
        return NULL;
    }
    if (size >= WARNING_LARGE_REQUEST_SIZE
        && ((flags & HEAP_ALLOW_PAGING) == 0 || (flags & HEAP_NO_FAULT) != 0)) {
        LogDeveloperWarning("AllocHeapEx called with allocation of size 0x%X."
            "Consider using MapVirt.\n", size
        );
    }

    bool acquired = LockHeap(flags & HEAP_ALLOW_PAGING);
    size = RoundUpSize(size);
    struct block* block = FindBlock(size, flags);

#ifndef NDEBUG
    outstanding_allocations++;
#endif

    if (acquired) {
        UnlockHeap(flags & HEAP_ALLOW_PAGING);
    }

    assert(((size_t) block & (ALIGNMENT - 1)) == 0);

    void* ptr = AddVoidPtr(block, METADATA_LEADING);
    if (flags & HEAP_ZERO) {
        inline_memset(ptr, 0, size);
    }

    return ptr;
}

void* AllocHeap(size_t size) {
    return AllocHeapEx(size, HEAP_NO_FAULT);
}

void* AllocHeapZero(size_t size) {
    return AllocHeapEx(size, HEAP_ZERO | HEAP_NO_FAULT);
}

void FreeHeap(void* ptr) {
    MAX_IRQL(IRQL_SCHEDULER);

    if (ptr == NULL) {
        return;
    }

    struct block* block = SubVoidPtr(ptr, METADATA_LEADING);
    bool pagable = IsOnSwappableHeap(block);
    block->prev = NULL;
    block->next = NULL;

    bool acquired = LockHeap(pagable);

    AddBlock(block);

#ifndef NDEBUG
    outstanding_allocations--;
#endif

    if (acquired) {
        UnlockHeap(pagable);
    }
}

void ReinitHeap(void) {
    heap_lock = CreateMutex("heap");
}

void InitHeap(void) {
    InitSpinlock(&heap_spinlock, "heapspin", IRQL_SCHEDULER); 
    InitSpinlock(&heap_locker_lock, "locker", IRQL_HIGH);
    MarkTfwStartPoint(TFW_SP_AFTER_HEAP);
}
