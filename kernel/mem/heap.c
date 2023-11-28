#include <common.h>
#include <assert.h>
#include <stdbool.h>
#include <panic.h>
#include <virtual.h>
#include <arch.h>
#include <string.h>
#include <spinlock.h>
#include <heap.h>
#include <log.h>
#include <irql.h>

#define BOOTSTRAP_AREA_SIZE (1024 * 16)
#define MAX_EMERGENCY_BLOCKS 16

/*
 * For requests larger than this, we'll issue a warning to say that MapVirt is a much better choice.
 */
#define WARNING_LARGE_REQUEST_SIZE (1024 * 3 + 512)

struct emergency_block {
    uint8_t* address;
    size_t size;
    bool valid;
};

/**
 * Used as a system block that can be used even before the virtual memory manager is setup.
 */
static uint8_t bootstrap_memory_area[BOOTSTRAP_AREA_SIZE] __attribute__ ((aligned(ARCH_PAGE_SIZE)));

/*
 * Used to give us memory when we are not allowed to fault (i.e. can't allocate virtual memory).
 */
static struct emergency_block emergency_blocks[MAX_EMERGENCY_BLOCKS] = {
    {.address = bootstrap_memory_area, .size = BOOTSTRAP_AREA_SIZE, .valid = true}
};

static struct spinlock heap_lock;

static void* AllocateFromEmergencyBlocks(size_t size) {
    int smallest_block = -1;

    for (int i = 0; i < MAX_EMERGENCY_BLOCKS; ++i) {
        if (emergency_blocks[i].valid && emergency_blocks[i].size >= size) {
            if (smallest_block == -1 || emergency_blocks[i].size < emergency_blocks[smallest_block].size) {
                smallest_block = i;
            }
        }
    }

    if (smallest_block == -1) {
        Panic(PANIC_OUT_OF_BOOTSTRAP_HEAP);
    }

    void* address = emergency_blocks[smallest_block].address;

    LogWriteSerial("Grabbing from emo block of size 0x%X, for request 0x%X\n", emergency_blocks[smallest_block].size, size);

    emergency_blocks[smallest_block].address += size;
    emergency_blocks[smallest_block].size -= size;

    if (emergency_blocks[smallest_block].size < ARCH_PAGE_SIZE) {
        emergency_blocks[smallest_block].valid = false;
    }

    return address;
}

static void AddBlockToBackupHeap(size_t size) {
    void* address = (void*) MapVirt(0, 0, size, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);

    int index_of_smallest_block = 0;
    for (int i = 0; i < MAX_EMERGENCY_BLOCKS; ++i) {
        if (emergency_blocks[i].valid) {
            if (emergency_blocks[i].size < emergency_blocks[index_of_smallest_block].size) {
                index_of_smallest_block = i;
            }
        } else {
            emergency_blocks[i].valid = true;
            emergency_blocks[i].size = size;
            emergency_blocks[i].address = address;
            return;
        }
    }

    LogDeveloperWarning("losing 0x%X bytes due to strangeness with backup heap.\n", emergency_blocks[index_of_smallest_block].size);
    LogDeveloperWarning("TODO: could probably just add this as a regular block to the regular heap.\n");

    emergency_blocks[index_of_smallest_block].size = size;
    emergency_blocks[index_of_smallest_block].address = address;
}

static void RestoreEmergencyPages(void* context) {
    (void) context;

    EXACT_IRQL(IRQL_STANDARD);

    /*
     * TODO: mabye make this greedier (i.e. grab larger blocks), but also have a way for the PMM to ask for larger
     * blocks back if needed. would still need to retain enough stashed away for PMM/VMM to use the heap, and would
     * need to unlock the pages, and ensure that allocations from emergency blocks are done via smallest-fit (so large
     * blocks aren't wasted).
     */

    size_t total_size = 0;
    size_t largest_block = 0;
    
    for (int i = 0; i < MAX_EMERGENCY_BLOCKS; ++i) {
        if (emergency_blocks[i].valid) {
            size_t size = emergency_blocks[i].size;
            total_size += size;
            if (size > largest_block) {
                largest_block = size;
            }
        }
    }

    LogWriteSerial("backup heap: largest is 0x%X, total is 0x%X\n", largest_block, total_size);

    while (largest_block < BOOTSTRAP_AREA_SIZE / 2 || total_size < BOOTSTRAP_AREA_SIZE) {
        AddBlockToBackupHeap(BOOTSTRAP_AREA_SIZE);
        total_size += BOOTSTRAP_AREA_SIZE;
        largest_block = BOOTSTRAP_AREA_SIZE;
    }
}

/**
 * Represents a section of memory that is either allocated or free. The memory address
 * it represents is itself, excluding the metadata at the start or end.
 *
 * See the report for further details.
 */
struct block {
    /*
     * The entire size of the block. The low 2 bits do not form part of the size,
     * the low bit is set for allocated blocks, and the second lowest bit is indicates
     * if it is on the swappable heap or not.
     */
    size_t size;

    /*
     * Only here on free blocks. Allocated blocks use this as the start of allocated
     * memory.
     */
    struct block* next;
    struct block* prev;

    /*
     * At position size - sizeof(size_t), there is the trailing size tag.
     * There are no flags in the low bit of this value, unlike the heading size tag.
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
 * The amount of metadata at the start and end of allocated blocks. The next and free
 * pointers in free blocks do not count.
 */
#define METADATA_LEADING_AMOUNT (sizeof(size_t))
#define METADATA_TRIALING_AMOUNT (sizeof(size_t))
#define METADATA_TOTAL_AMOUNT (METADATA_LEADING_AMOUNT + METADATA_TRIALING_AMOUNT)

/**
 * (THIS COMMENT IS ABOUT x86-64 - halve the byte values for x86)
* Having blocks of size 8 is wasteful, as they need to be at least 32 bytes long total to fit
* the metadata when free, but only need 16 bytes metadata when allocated. Therefore, we have
* 8 spare bytes that are wasted.
*/
#define MINIMUM_REQUEST_SIZE_INTERNAL (2 * sizeof(size_t))

/**
 * NUM_INCREMENTAL_FREE_LISTS = how many of MINIMUM_REQUEST_SIZE_INTERNAL + ALIGNMENT * N we have
 * NUM_EXPONENTIAL_FREE_LISTS = how many of RoundUpPower2(MINIMUM_REQUEST_SIZE_INTERNAL + (ALIGNMENT * NUM_INCREMENTAL_FREE_LISTS)) << N
 * The last size should be larger than the max allocation size, as otherwise we could allocate larger than we have in the final list.
 */
#define TOTAL_NUM_FREE_LISTS 35

/**
 * An array which holds the minimum allocation sizes that each free list can hold.
 */
static size_t free_list_block_sizes[TOTAL_NUM_FREE_LISTS] = {
    16,
    24,
    32,
    40,
    48,
    56,
    64,
    72,
    80,                 
    88,                 // 10
    96,
    104,
    112,
    120,
    128,
    160,
    192,
    224,
    256,                
    320,                // 20
    384,
    448,
    512,
    768,
    1024,
    1536,
    2048,
    1024 * 3,
    1024 * 4,           
    1024 * 8,           // 30
    1024 * 16,          // 31
    1024 * 32,          // 32
    1024 * 64,          // 33
    1024 * 128,         // 34
    1024 * 256,         // 35
};

/**
 * Used to work out which free list a block should be in, when we are *reading* a block.
 * This rounds the size *up*, meaning it cannot be used to insert a block into a list.
 * NOT USED TO INSERT BLOCKS!! 
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
 * Calculates which free list a block should be inserted in. This one rounds *down*, and
 * so it should not normally be used to look up where a block should be.
 */
static int GetInsertionIndex(size_t size_without_metadata) {
    /*
     * This will only fail if we somehow get a block that is smaller than the minimum
     * possible size (i.e., something has gone very wrong.)
     */

    assert(size_without_metadata >= free_list_block_sizes[0]);

    /*
     * We can't round down to the next one when it's the smallest possible size,
     * so handle this case specially.
     */
    if (size_without_metadata == free_list_block_sizes[0]) {
        return 0;
    }
  
    /*
     * Look until we go past the block size we need, and then return the previous
     * value. This gives us the final block size that doesn't exceed the input value.
     */
    for (int i = 0; i < TOTAL_NUM_FREE_LISTS - 1; ++i) {
        if (size_without_metadata <= free_list_block_sizes[i]) {
            return i - 1;
        }
    }

    Panic(PANIC_HEAP_REQUEST_TOO_LARGE);
}

/**
 * Adding to a void pointer is undefined behaviour, so this gets around that by
 * casting to a byte pointer.
 */
static void* AddVoidPtr(void* ptr, size_t offset) {
    return (void*) (((uint8_t*) ptr) + offset);
}

/**
 * Subtracting from a void pointer is undefined behaviour, so this gets around that by
 * casting to a byte pointer.
 */
static void* SubVoidPointer(void* ptr, size_t offset) {
    return (void*) (((uint8_t*) ptr) - offset);
}

/**
 * Rounds up a user-supplied allocation size to the alignment. If the value is smaller than
 * the minimum request size that is internally supported, it will round it up to that size.
 */
static size_t RoundUpSize(size_t size) {
    /* Should have already been checked against. */
    assert(size != 0);

    if (size < MINIMUM_REQUEST_SIZE_INTERNAL) {
        size = MINIMUM_REQUEST_SIZE_INTERNAL;
    }

    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

/**
 * Marks a block as being free (unallocated).
 */
static void MarkFree(struct block* block) {
    block->size &= ~1;
}

/**
 * Marks a block as being allocated.
 */
static void MarkAllocated(struct block* block) {
    block->size |= 1;
}

/**
 * Returns true if the block is allocated, or false if it is free. 
 */
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
 * Entries in free lists must have a user allocated size GREATER OR EQUAL TO the size in free_list_block_sizes.
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
 * Given a block, returns its total size, including metadata. This takes into account
 * the flags on the size field and removes them from the return value. 
 */
static size_t GetBlockSize(struct block* block) {
    size_t size = block->size & ~3;

    /*
     * Ensure the other size tag matches. If it doesn't, there has been memory corruption.
     */
    assert(*(((size_t*) block) + (size / sizeof(size_t)) - 1) == size);
    return size;
}

void DbgPrintListStats(void) {
    for (int i = 0; i < TOTAL_NUM_FREE_LISTS; ++i) {
        struct block* unswap = GetHeap(false)[i];
        struct block* swap = GetHeap(true)[i];

        size_t unswap_blocks = 0;
        size_t unswap_size = 0;
        size_t swap_blocks = 0;
        size_t swap_size = 0;

        while (unswap) {
            ++unswap_blocks;
            unswap_size += GetBlockSize(unswap);
            unswap = unswap->next;
        }

        while (swap) {
            ++swap_blocks;
            swap_size += GetBlockSize(swap);
            swap = swap->next;
        }

        if ((unswap_blocks | swap_blocks) != 0) {
            LogWriteSerial("Bucket %d [0x%X]: unswappable %d / 0x%X. swappable %d / 0x%X\n", i, free_list_block_sizes[i], unswap_blocks, unswap_size, swap_blocks, swap_size);
        }
    }
    LogWriteSerial("\n");
}

/**
 * Sets the *total* size of a given block. This does not do any checking, so the caller must be
 * careful to ensure that calling this doesn't leak memory (by setting it too small), or cause
 * double-allocation of the same area (by setting it too large). Carries the swappability and allocation
 * tags with it from the front to the back tags.
 */
static void SetSizeTags(struct block* block, size_t size) {
    block->size = (block->size & 3) | size;
    *(((size_t*) block) + (size / sizeof(size_t)) - 1) = size;
}

static void* GetSystemMemory(size_t size, int flags) {
    EXACT_IRQL(IRQL_SCHEDULER);

    if (flags & HEAP_NO_FAULT) {
        if (flags & HEAP_ALLOW_PAGING) {
            PanicEx(PANIC_OUT_OF_BOOTSTRAP_HEAP, "HEAP_NO_FAULT was set alongside HEAP_ALLOW_PAGING");
        }
        if (IsVirtInitialised()) {
            DeferUntilIrql(IRQL_STANDARD, RestoreEmergencyPages, NULL);
        }
        return AllocateFromEmergencyBlocks(size);
    }

    return (void*) MapVirt(0, 0, size, VM_READ | VM_WRITE | (flags & HEAP_ALLOW_PAGING ? 0 : VM_LOCK), NULL, 0);
}

/**
 * Allocates a new block from the system that is able to hold the amount of data
 * specified. Also allocated enough memory for fenceposts on either side of the data,
 * and sets up these fenceposts correctly.
 */
static struct block* RequestBlock(size_t total_size, int flags) {
    EXACT_IRQL(IRQL_SCHEDULER);

    /*
     * We need to add the extra bytes for fenceposts to be added. We must do this before we
     * round up to the nearest areana size (if we did it after, it wouldn't be aligned anymore).
     */
    total_size += MINIMUM_REQUEST_SIZE_INTERNAL * 2;
    total_size = (total_size + ARCH_PAGE_SIZE - 1) & ~(ARCH_PAGE_SIZE - 1);

    if (!IsVirtInitialised()) {
        flags |= HEAP_NO_FAULT;
    }

    /*
     * Get memory from the system.
     */
    struct block* block = (struct block*) GetSystemMemory(total_size, flags);
    if (block == NULL) {
        Panic(PANIC_OUT_OF_HEAP);
    }

    /*
     * Set the metadata for both the fenceposts and the main data block. 
     * Keep in mind that total_size now includes the fencepost metadata (see top of function), so this
     * sometimes needs to be subtracted off.
     */
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

    MarkSwappability(left_fence, flags & HEAP_ALLOW_PAGING);
    MarkSwappability(right_fence, flags & HEAP_ALLOW_PAGING);
    MarkSwappability(actual_block, flags & HEAP_ALLOW_PAGING);

    return actual_block;
}

/**
 * Removes a block from a free list. It needs to take in the exact free list's index (as opposed to calculating
 * it itself), as this may be used halfway though allocations or deallocations where the block isn't yet in
 * its correct block.
 */
static void RemoveBlock(int free_list_index, struct block* block) {
    EXACT_IRQL(IRQL_SCHEDULER);

    struct block** head_list = GetHeapForBlock(block);

    /*
     * Perform a standard linked list deletion. 
     */
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


/*static void DbgCheckNeighbours(struct block* block) {
    size_t size = GetBlockSize(block);
    LogWriteSerial("    Block @ 0x%X is of size %d, and %s\n", block, size, IsAllocated(block) ? "allocated" : "free");
    size_t prev_block_size = *(((size_t*) block) - 1);
    struct block* prev_block = (struct block*) (((size_t*) block) - prev_block_size / sizeof(size_t));
    struct block* next_block = (struct block*) (((size_t*) block) + size / sizeof(size_t));

    LogWriteSerial("    Prev @ 0x%X is of size %d, and %s\n", prev_block, GetBlockSize(prev_block), IsAllocated(prev_block) ? "allocated" : "free");
    LogWriteSerial("    Next @ 0x%X is of size %d, and %s\n\n", next_block, GetBlockSize(next_block), IsAllocated(next_block) ? "allocated" : "free");
}*/

/**
 * Adds a block to its appropriate free list. It also coalesces the block with surrounding free blocks
 * if possible.
 */
static struct block* AddBlock(struct block* block) {
    EXACT_IRQL(IRQL_SCHEDULER);

    /*
     * Although this function is technically recursive (because it needs to shuffle blocks into different
     * lists by calling itself again), but there are a constant number of free lists, so it is still
     * coalescing in constant time.
     */
    size_t size = GetBlockSize(block);
    struct block** head_list = GetHeapForBlock(block);

    int free_list_index = GetInsertionIndex(size - METADATA_TOTAL_AMOUNT);

    size_t prev_block_size = *(((size_t*) block) - 1);
    struct block* prev_block = (struct block*) (((size_t*) block) - prev_block_size / sizeof(size_t));
    struct block* next_block = (struct block*) (((size_t*) block) + size / sizeof(size_t));

    if (IsAllocated(prev_block) && IsAllocated(next_block)) {
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

    } else if (IsAllocated(prev_block) && !IsAllocated(next_block)) {
        /*
         * Need to coalesce with the one on the right.
         */

        /*
         * Swappable and non-swappable blocks should be on entirely seperate heaps, and you can't look into
         * the other because the fences should prevent anyone looking between them.
         */
        bool swappable = IsOnSwappableHeap(block);
        assert(swappable == IsOnSwappableHeap(next_block));
    
        RemoveBlock(GetInsertionIndex(GetBlockSize(next_block) - METADATA_TOTAL_AMOUNT), next_block);
        SetSizeTags(block, size + GetBlockSize(next_block));
        block->prev = NULL;
        block->next = NULL;
        MarkFree(block);
        MarkSwappability(block, swappable);

        return AddBlock(block);
    
    } else if (!IsAllocated(prev_block) && IsAllocated(next_block)) {
        /*
         * Need to coalesce with the one on the left.
         */

         /*
         * Swappable and non-swappable blocks should be on entirely seperate heaps, and you can't look into
         * the other because the fences should prevent anyone looking between them.
         */
        bool swappable = IsOnSwappableHeap(block);
        assert(swappable == IsOnSwappableHeap(prev_block));

        RemoveBlock(GetInsertionIndex(GetBlockSize(prev_block) - METADATA_TOTAL_AMOUNT), prev_block);
        SetSizeTags(prev_block, size + GetBlockSize(prev_block));
        prev_block->prev = NULL;
        prev_block->next = NULL;
        MarkFree(prev_block);
        MarkSwappability(prev_block, swappable);

        return AddBlock(prev_block);

    } else {
        /*
         * Coalesce with blocks on both sides.
         */

        /*
         * Swappable and non-swappable blocks should be on entirely seperate heaps, and you can't look into
         * the other because the fences should prevent anyone looking between them.
         */
        bool swappable = IsOnSwappableHeap(block);
        assert(swappable == IsOnSwappableHeap(prev_block));
        assert(swappable == IsOnSwappableHeap(next_block));

        RemoveBlock(GetInsertionIndex(GetBlockSize(prev_block) - METADATA_TOTAL_AMOUNT), prev_block);
        RemoveBlock(GetInsertionIndex(GetBlockSize(next_block) - METADATA_TOTAL_AMOUNT), next_block);

        SetSizeTags(prev_block, size + GetBlockSize(prev_block) + GetBlockSize(next_block));
        prev_block->prev = NULL;
        prev_block->next = NULL;
        MarkFree(prev_block);
        MarkSwappability(prev_block, swappable);

        return AddBlock(prev_block);
    }
}

/*
 * Allocates a block. The block to be allocated will be the first block in the given free
 * list, and that free list must be non-empty, and be able to fit the requested size.
 */
static struct block* AllocateBlock(struct block* block, int free_list_index, size_t user_requested_size) {
    EXACT_IRQL(IRQL_SCHEDULER);
    assert(block != NULL);

    size_t total_size = user_requested_size + METADATA_TOTAL_AMOUNT;
    size_t block_size = GetBlockSize(block);

    assert(block_size >= total_size);

    if (block_size - total_size < MINIMUM_REQUEST_SIZE_INTERNAL + METADATA_TOTAL_AMOUNT) {
        /*
         * We can just remove from the list altogether if the sizes match up exactly,
         * or if there would be so little left over that we can't form a new block.
         */
        RemoveBlock(free_list_index, block);
        /*
         * Prevent memory leak (from having a hole in memory), but do it after removing
         * the block, as this may change the list it needs to be in, and RemoveBlock
         * will not like that.
         */
        SetSizeTags(block, block_size);
        MarkAllocated(block);
        return block;

    } else {
        /*
         * We must split the block into two. If no list change is needed, we can leave the 'leftover' parts in the list
         * as is (just fixing up the size tags), and then return the new block.
         */

        RemoveBlock(free_list_index, block);

        size_t leftover = block_size - total_size;
        SetSizeTags(block, leftover);

        struct block* allocated_block = (struct block*) (((size_t*) block) + (leftover / sizeof(size_t)));
        SetSizeTags(allocated_block, total_size);

        /*
         * We are giving it new tags, so must set this correctly.
         */
        MarkSwappability(allocated_block, IsOnSwappableHeap(block));

        /*
         * Must be done before we try to move around the leftovers (or else it will actually
         * coalesce back into one block). 
         */
        MarkAllocated(allocated_block);

        /*
        * We need to remove the leftover block from this list, and add it to the correct list.
        */
        MarkFree(block);
        AddBlock(block);
        return allocated_block;
    }
}

/**
 * Allocates a block that can fit the user requested size. It will request new memory from the
 * system if required. If it returns NULL, then there is not enough memory of the system to
 * satisfy the request.
 */
static struct block* FindBlock(size_t user_requested_size, int flags) {
    EXACT_IRQL(IRQL_SCHEDULER);

    struct block** head_list = GetHeap(flags & HEAP_ALLOW_PAGING);
    /*
     * Check the free lists in order, starting from the smallest one that will fit the block.
     * If we find one with a free block, then we allocate the head of that list.
     */
    int min_index = GetSmallestListIndexThatFits(user_requested_size);

    for (int i = min_index; i < TOTAL_NUM_FREE_LISTS; ++i) {
        if (head_list[i] != NULL) {
            return AllocateBlock(head_list[i], i, user_requested_size);
        }
    }

    /*
     * If we want paging (but not forcing it), but couldn't get it, try again without it.
     * (that sentence I think was even more confusing than the code)
     */
    if ((flags & HEAP_ALLOW_PAGING) && !(flags & HEAP_FORCE_PAGING)) {
        return FindBlock(user_requested_size, flags & ~HEAP_ALLOW_PAGING);
    }

    /*
     * If we can't find a block that will fit, then we must allocate more memory.
     */
    size_t total_size = user_requested_size + METADATA_TOTAL_AMOUNT;
    struct block* sys_block = RequestBlock(total_size, flags);

    /*
     * Put the new memory in the free list (which ought to be empty, as wouldn't need to
     * request new memory otherwise). Then we can allocate the block.
     */
    int sys_index = GetInsertionIndex(GetBlockSize(sys_block) - METADATA_TOTAL_AMOUNT);
    assert(head_list[sys_index] == NULL);
    head_list[sys_index] = sys_block;
    return AllocateBlock(head_list[sys_index], sys_index, user_requested_size);
}

/**
 * Allocates memory on the heap. Unless you *really* know what you're doing, you should always
 * pass HEAP_NO_FAULT. AllocHeap passes this automatically, but this one doesn't (in case you
 * want to allocate from the pagable pool).
 */
void* AllocHeapEx(size_t size, int flags) {
    MAX_IRQL(IRQL_SCHEDULER);

    /*
     * We cannot allocate zero blocks (as it would be useless, and couldn't be freed.)
     * Size cannot be negative as a size_t is an unsigned type.
     */
    if (size == 0) {
        return NULL;
    }

    if (size >= WARNING_LARGE_REQUEST_SIZE) {
        LogDeveloperWarning("AllocHeapEx called with allocation of size 0x%X. You should seriously consider using MapVirt.\n", size);
    }

    size = RoundUpSize(size);

    if (flags & HEAP_FORCE_PAGING) {
        flags |= HEAP_ALLOW_PAGING;
    }

    int irql = AcquireSpinlock(&heap_lock, true);
    struct block* block = FindBlock(size, flags);

#ifndef NDEBUG
    outstanding_allocations++;
#endif

    ReleaseSpinlockAndLower(&heap_lock, irql);

    assert(((size_t) block & (ALIGNMENT - 1)) == 0);

    void* ptr = AddVoidPtr(block, METADATA_LEADING_AMOUNT);
    if (flags & HEAP_ZERO) {
        memset(ptr, 0, size);
    }

    DbgPrintListStats();

    return ptr;
}

void* AllocHeap(size_t size) {
    MAX_IRQL(IRQL_SCHEDULER);
    return AllocHeapEx(size, HEAP_NO_FAULT);
}

void* AllocHeapZero(size_t size) {
    MAX_IRQL(IRQL_SCHEDULER);
    return AllocHeapEx(size, HEAP_ZERO | HEAP_NO_FAULT);
}

void FreeHeap(void* ptr) {
    MAX_IRQL(IRQL_SCHEDULER);

    /*
     * Guard against NULL, as the standard says: "If ptr is a null pointer, no action occurs"
     */
    if (ptr == NULL) {
        return;
    }
    
    struct block* block = SubVoidPointer(ptr, METADATA_LEADING_AMOUNT);
    block->prev = NULL;
    block->next = NULL;

    int irql = AcquireSpinlock(&heap_lock, true);
    AddBlock(block);

#ifndef NDEBUG
    outstanding_allocations--;
#endif
    ReleaseSpinlockAndLower(&heap_lock, irql);
}

/*
 * Completely untested. 
 */
void* ReallocHeap(void* ptr, size_t new_size) {
    MAX_IRQL(IRQL_SCHEDULER);

    if (ptr == NULL || new_size == 0) {
        LogDeveloperWarning("do not call ReallocHeap() with a NULL pointer or zero size!\n");
        return NULL;
    }

    struct block* block = SubVoidPointer(ptr, METADATA_LEADING_AMOUNT);
    size_t old_size = GetBlockSize(block);
    new_size += METADATA_TOTAL_AMOUNT;

    if (new_size == old_size) {
        return ptr;

    } else if (new_size < old_size) {
        /*
         * Not enough space to create a free block (sure, it may be possible to coalesce,
         * but who really cares - that extra space will still be able to be allocated by someone
         * else later on.
         */
        size_t freed_bytes = old_size - new_size;
        if (freed_bytes < MINIMUM_REQUEST_SIZE_INTERNAL) {
            return ptr;
        }
        
        int irql = AcquireSpinlock(&heap_lock, true);

        /*
         * Decrease the size of the current block, and add the leftovers as a new free block.
         */
        struct block* freed_area = (struct block*) (((size_t*) block) + (new_size / sizeof(size_t)));
        SetSizeTags(block, new_size);
        SetSizeTags(freed_area, freed_bytes);
        MarkFree(freed_area);
        AddBlock(freed_area);

        ReleaseSpinlockAndLower(&heap_lock, irql);

        return ptr;

    } else {
        /*
         * We need to expand the block.
         */
        size_t bytes_to_expand_by = new_size - old_size;

        struct block* next_block = (struct block*) (((size_t*) block) + old_size / sizeof(size_t));
        size_t next_block_size = GetBlockSize(next_block);
        size_t remainder_in_next = next_block_size - bytes_to_expand_by;

        if (!IsAllocated(next_block) && next_block_size >= bytes_to_expand_by) {
            int irql = AcquireSpinlock(&heap_lock, true);

            RemoveBlock(GetInsertionIndex(GetBlockSize(next_block) - METADATA_TOTAL_AMOUNT), next_block);

            /*
             * If there is only going to be a tiny bit left in the next block after we allocate,
             * we should just eat the entire next block.
             */
            if (remainder_in_next < METADATA_TOTAL_AMOUNT) {
                SetSizeTags(block, old_size + next_block_size);

            } else {
                SetSizeTags(block, new_size);
                SetSizeTags(next_block, remainder_in_next);
                MarkFree(next_block);
                AddBlock(next_block);
            }

            ReleaseSpinlockAndLower(&heap_lock, irql);
            
            return ptr;

        } else {
            void* new_ptr = AllocHeap(new_size - METADATA_TOTAL_AMOUNT);
            memcpy(ptr, new_ptr, new_size - METADATA_TOTAL_AMOUNT);
            FreeHeap(ptr);
            return new_ptr;
        }
    }
}

void InitHeap(void) {
    InitSpinlock(&heap_lock, "heap", IRQL_SCHEDULER);
}

