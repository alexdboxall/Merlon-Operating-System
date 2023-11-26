
/*
 * mem/physical.c - Physical Memory Manager
 *
 * There are two allocation systems in use here. The first is a bitmap system, which has a
 * bit for each page, which when set, indicates that a page is free. This can be used before
 * virtual memory is available, and provides O(n) allocation time. After virtual memory is 
 * available, a stack-based system is used to provide O(1) allocation time. The bitmap is still
 * kept in sync with the stack, to allow detection of double-free conditions and for other uses.
 *
 * When physical memory is low, we evict pages before we reach the out of memory condition. This
 * allows eviction code to allocate physical memory without running out of memory.
 */

#include <arch.h>
#include <physical.h>
#include <common.h>
#include <spinlock.h>
#include <assert.h>
#include <string.h>
#include <irql.h>
#include <log.h>
#include <virtual.h>
#include <panic.h>

/*
 * How many bits are in each entry of the bitmap allocation array. Should be the number
 * of bits in a size_t.
 */
#define BITS_PER_ENTRY (sizeof(size_t) * 8)

/*
 * The maximum physical memory address we can use, in kilobytes. 
 */
#define MAX_MEMORY_KBS ARCH_MAX_RAM_KBS

/*
 * The number of pages required to reach the maximum physical memory address.
 */
#define MAX_MEMORY_PAGES (MAX_MEMORY_KBS / ARCH_PAGE_SIZE * 1024) 

/*
 * The number of entries in the bitmap allocation table required to keep track of
 * any page up to the maximum usable physical memory address.
 */
#define BITMAP_ENTRIES (MAX_MEMORY_PAGES / BITS_PER_ENTRY)

/*
 * We will start evicting pages once we have fewer than this many pages left on the system.
 * We can have less than this available, if in the process of eviction it causes more pages
 * to be allocated (this is why we set it to something higher than 0 or 1, to provide a buffer
 * for eviction to work in).
 */
#define NUM_EMERGENCY_PAGES 32

/*
 * One bit per page. Lower bits refer to lower pages. A clear bit indicates
 * the page is unavailable (allocated / non-RAM), and a set bit indicates the
 * page is free.
 */
static size_t allocation_bitmap[BITMAP_ENTRIES];

/*
 * Stores pages that are available for us to allocate. If set to NULL, then we have yet to
 * reinitialise the physical memory manager, and so it cannot be used. The stack grows
 * upward, and the pointer is incremented after writing the value on a push. The stack stores
 * physical page numbers (indexes) instead of addresses.
 */
static size_t* allocation_stack = NULL;

/*
 * The index in to the allocation_stack bitmap where the next push operation will put
 * the value.
 */
static size_t allocation_stack_pointer = 0;

/*
 * The number of physical pages available (free) remaining in the system. Gets adjusted 
 * on each allocation or deallocation. Gets set during InitPhys() when scanning the system's
 * memory map.
 */
static size_t pages_left = 0;

/*
 * The total number of allocatable pages on the system. Gets set during InitPhys() and ReinitPhys()
 */
static size_t total_pages = 0;

/*
 * The highest physical page number that exists on this system. Gets set during InitPhys() when
 * scanning the system's memory map.
 */
static size_t highest_valid_page_index = 0;

/*
 * A lock to prevent concurrent access to the physical memory manager. AllocPhys() is allowed to
 * cause page faults, which can lead to recursive use of this lock. Therefore, this lock should
 * be acquired with RecursiveAcquireSpinlock() instead of AcquireSpinlock()
 */
static struct spinlock phys_lock;

static inline bool IsBitmapEntryFree(size_t index) {
    size_t base = index / BITS_PER_ENTRY;
    size_t offset = index % BITS_PER_ENTRY;

    return allocation_bitmap[base] & (1 << offset);
}

static inline void AllocateBitmapEntry(size_t index) {
    size_t base = index / BITS_PER_ENTRY;
    size_t offset = index % BITS_PER_ENTRY;

    assert(IsBitmapEntryFree(index));

    allocation_bitmap[base] &= ~(1 << offset);
}

static inline void DeallocateBitmapEntry(size_t index) {
    size_t base = index / BITS_PER_ENTRY;
    size_t offset = index % BITS_PER_ENTRY;

    assert(!IsBitmapEntryFree(index));

    allocation_bitmap[base] |= 1 << offset;
}

static inline void PushIndex(size_t index) {
    assert(index <= highest_valid_page_index);
    allocation_stack[allocation_stack_pointer++] = index;
}

static inline size_t PopIndex(void) {
    assert(allocation_stack_pointer != 0);
    return allocation_stack[--allocation_stack_pointer];
}

/*
 * Removes an entry from the stack by value. Only to be used when absolutely required,
 * as it has O(n) runtime and is therefore very slow. 
 */
static void RemoveStackEntry(size_t index) {
    for (size_t i = 0; i < allocation_stack_pointer; ++i) {
        if (allocation_stack[i] == index) {
            memmove(allocation_stack + i, allocation_stack + i + 1, (--allocation_stack_pointer - i) * sizeof(size_t));
            return;
        }
    }
}

/**
 * Deallocates a page of physical memory that was allocated with AllocPhys(). Does not affect virtual mappings -
 * that should be taken care of before deallocating.
 *
 * @param addr The address of the page to deallocate. Must be page-aligned.
 */
void DeallocPhys(size_t addr) {
    MAX_IRQL(IRQL_SCHEDULER);
    assert(addr % ARCH_PAGE_SIZE == 0);

    size_t page = addr / ARCH_PAGE_SIZE;

    int irql = AcquireSpinlock(&phys_lock, true);

    ++pages_left;
    DeallocateBitmapEntry(page);
    if (allocation_stack != NULL) {
        PushIndex(page);
    }

    ReleaseSpinlockAndLower(&phys_lock, irql);
}

/**
 * Deallocates a section of physical memory that was allocated with AllocPhysContinuous(). The entire block
 * of memory must be deallocated at once, i.e. the start address of the memory should be passed in. Does not
 * affect virtual mappings - that should be taken care of before deallocating.
 * 
 * @param addr The address of the section of memory to deallocate. Must be page-aligned.
 * @param size The size of the allocation. This should be the same value that was passed into AllocPhysContinuous().
 */
void DeallocPhysContiguous(size_t addr, size_t bytes) {
    MAX_IRQL(IRQL_SCHEDULER);

    size_t pages = (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
    for (size_t i = 0; i < pages; ++i) {
        DeallocPhys(addr);
        addr += ARCH_PAGE_SIZE;
    }
}

static void EvictPagesIfNeeded(void* context) {
    (void) context;

    EXACT_IRQL(IRQL_STANDARD);

    /*
    * We can't fault later on, so we evict now if we are getting low on memory. If this faults,
    * the recursion will not cause the spinlock to be re-acquired, and so the evicted code won't
    * run again either - this prevents infinite recurison loops. These fault handlers and recursive
    * calls can allocate and make use of these 'emergency' pages that we keep by doing this eviction
    * before we actually run out of memory.
    *
    * We loop so that if the first evictions end up needing to allocate memory, we can hopefully
    * perform another eviction to make up for it (that shouldn't need extra memory).
    */

    // TODO: probs needs lock on pages_left
    int timeout = 0;
    while (pages_left < NUM_EMERGENCY_PAGES && timeout < 10) {
        LogWriteSerial("Evicting virt... (got %d pages left)\n", pages_left);
        EvictVirt();
        ++timeout;
    }

    if (pages_left == 0) {
       Panic(PANIC_OUT_OF_PHYS);
    }
}

/**
 * Allocates a page of physical memory. The resulting memory can be freed with DeallocPhys(). May cause
 * pages to be evicted from RAM in order to have to enough physical memory. Direct users of this function
 * should be careful - any physical pages that have been allocated but not mapped into virtual memory
 * cannot be swapped out, and therefore they should be mapped into virtual memory.
 *
 * @return The start address of the page of physical memory, or 0 if a page could not be allocated.
 */
size_t AllocPhys(void) {
    MAX_IRQL(IRQL_SCHEDULER);

    DeferUntilIrql(IRQL_STANDARD, EvictPagesIfNeeded, NULL);

    int irql = AcquireSpinlock(&phys_lock, true);

    size_t index = 0;
    if (allocation_stack == NULL) {
        /*
         * No stack yet, so must use the bitmap. We could optimise and keep track of the most recently
         * returned index, but this code is only used during InitVirt(), so we'll try to keep the code
         * here short and simple.
         */
        while (!IsBitmapEntryFree(index)) {
            index = (index + 1) % MAX_MEMORY_PAGES;
        }
    } else {
        index = PopIndex();
    }

    AllocateBitmapEntry(index);
    --pages_left;

    ReleaseSpinlockAndLower(&phys_lock, irql);

    return index * ARCH_PAGE_SIZE;
}

/**
 * Allocates a section of contigous physical memory, that may or may not have requirements as
 * to where the memory can be located. Allocation in this way is very slow, and so should only
 * be called where absolutely necessary (e.g. initialising drivers). Must only be called after
 * a call to ReinitPhys() is made. Deallocation should be done by DeallocPhysContiguous(), passing
 * in the same size value as passed into AllocPhysContiguous() on allocation. Will not cause pages
 * to be evicted from RAM, so sufficient memory must exist on the system for this allocation to
 * succeed.
 *
 * @param bytes The size of the allocation, in bytes.
 * @param min_addr The allocated memory region will not contain any addresses that are lower than
 *                 this value.
 * @param max_addr The allocated memory region will not contain any addresses that are greater than
 *                 or equal to this value. If there is no maximum, set this to 0.
 * @param boundary The allocated memory will not contain any addresses that are an integer multiple 
 *                 of this value (although it may start at an integer multiple of this address).
 *                 If there are no boundary requirements, set this to 0.
 * @return The start address of the returned physical memory area. If the request could not be
 *         satisfied (e.g. out of memory, no contiguous block, cannot meet requirements), then 0
 *         is returned.
 */
size_t AllocPhysContiguous(size_t bytes, size_t min_addr, size_t max_addr, size_t boundary) {
    /*
     * This function should not be called before the stack allocator is setup.
     * (There is no need for InitVirt() to use this function, and so checking here removes
     * a check that would have to be done in a loop later).
     */
    if (allocation_stack == NULL) {
        return 0;
    }

    size_t pages = (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
    size_t min_index = (min_addr + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
    size_t max_index = max_addr == 0 ? highest_valid_page_index + 1 : max_addr / ARCH_PAGE_SIZE;
    size_t count = 0;

    int irql = AcquireSpinlock(&phys_lock, true);

    /*
     * We need to check we won't try to over-allocate memory, or allocate so much memory that it puts
     * us in a critical position.
     */
    if (pages + NUM_EMERGENCY_PAGES >= pages_left) {
        ReleaseSpinlockAndLower(&phys_lock, irql);
        return 0;
    }

    for (size_t index = min_index; index < max_index; ++index) {
        /*
         * Reset the counter if we are no longer contiguous, or if we have cross a boundary
         * that we can't cross.
         */
        if (!IsBitmapEntryFree(index) || (boundary != 0 && (index % (boundary / ARCH_PAGE_SIZE) == 0))) {
            count = 0;
            continue;
        }

        ++count;
        if (count == pages) {
            /*
             * Go back to the start of the section and mark it all as allocated.
             */
            size_t start_index = index - count + 1;
            while (start_index <= index) {
                AllocateBitmapEntry(start_index);
                RemoveStackEntry(start_index);
                ++start_index;
            }

            ReleaseSpinlockAndLower(&phys_lock, irql);
            return start_index * ARCH_PAGE_SIZE;
        }
    }

    ReleaseSpinlockAndLower(&phys_lock, irql);
    return 0;
}

/**
 * Initialises the physical memory manager for the first time. Must be called before any other
 * memory management function is called. It determines what memory is available on the system 
 * and prepares the O(n) bitmap allocator. This will be slow, but is only needed until ReinitHeap()
 * gets called. Must only be called once.
 */
void InitPhys(void) {
    InitSpinlock(&phys_lock, "phys", IRQL_SCHEDULER);

	/*
	* Scan the memory tables and fill in the memory that is there.
	*/
    int i = 0;
	while (true) {
		struct arch_memory_range* range = ArchGetMemory(i++);

		if (range == NULL) {
			/* No more memory exists */
			break;

		} else {
			/* 
			* Round conservatively (i.e., round the first page up, and the last page down)
			* so we don't accidentally allow non-existant memory to be allocated.
			*/
			size_t first_page = (range->start + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
			size_t last_page = (range->start + range->length) / ARCH_PAGE_SIZE;

			while (first_page < last_page && first_page < MAX_MEMORY_PAGES) {
                DeallocateBitmapEntry(first_page);
                ++pages_left;
                ++total_pages;

                if (first_page > highest_valid_page_index) {
                    highest_valid_page_index = first_page;
                }

                ++first_page;
			}
		}
	}
}

static void ReclaimBitmapSpace(void) {
    /*
     * We can save a tiny bit of extra physical memory on low-memory systems by deallocating the memory
     * in the bitmap that can't be reached (due to the system not having memory that goes up that high).
     * e.g. if we allow the system to access 16GB of RAM, but we only have 4MB, then we can save 31 pages
     * (or 124KB), which on a system with only 4MB of RAM is 3% of total physical memory).
     * 
     * Be careful! If we do this incorrectly we will get memory corruption and some *very* mysterious bugs.
     */

    size_t num_unreachable_pages = MAX_MEMORY_PAGES - (highest_valid_page_index + 1);
    size_t num_unreachable_entries = num_unreachable_pages / BITS_PER_ENTRY;
    size_t num_unreachable_bitmap_pages = num_unreachable_entries / ARCH_PAGE_SIZE;

    size_t end_bitmap = ((size_t) allocation_bitmap) + sizeof(allocation_bitmap);
    
    /*
     * DO NOT ROUND UP, or variables in the same page as the end of the bitmap will also be counted as 'free',
     * causing kernel memory corruption for whatever comes in RAM after that.
     */
    size_t unreachable_region = ((end_bitmap - ARCH_PAGE_SIZE * num_unreachable_bitmap_pages)) & ~(ARCH_PAGE_SIZE - 1);

    while (num_unreachable_bitmap_pages--) {
        DeallocPhys(ArchVirtualToPhysical(unreachable_region));
        unreachable_region += ARCH_PAGE_SIZE;
        ++total_pages;
    }
}

/**
 * Reinitialises the physical memory manager with a constant-time page allocation system.
 * Must be called after virtual memory has been initialised. Must only be called once. Must
 * be called before calling AllocPageContigous() is called. Should be called as soon as
 * possible after virtual memory is available.
 */
void ReinitPhys(void) {
    assert(allocation_stack == NULL);

    allocation_stack = (size_t*) MapVirt(0, 0, (highest_valid_page_index + 1) * sizeof(size_t), VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
    
    for (size_t i = 0; i < MAX_MEMORY_PAGES; ++i) {
        if (IsBitmapEntryFree(i)) {
            PushIndex(i);
        }
    }

    ReclaimBitmapSpace();
}


size_t GetTotalPhysKilobytes(void) {
    return total_pages * (ARCH_PAGE_SIZE / 1024);
}

size_t GetFreePhysKilobytes(void) {
    return pages_left * (ARCH_PAGE_SIZE / 1024);
}