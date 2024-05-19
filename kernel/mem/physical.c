
#include <arch.h>
#include <physical.h>
#include <diskcache.h>
#include <common.h>
#include <debug.h>
#include <spinlock.h>
#include <assert.h>
#include <string.h>
#include <irql.h>
#include <log.h>
#include <virtual.h>
#include <panic.h>

static struct spinlock phys_lock;

/*
 * One bit per page. Lower bits refer to lower pages. A clear bit indicates
 * the page is unavailable (allocated / non-RAM), and a set bit indicates the
 * page is free.
 */
#define MAX_MEMORY_PAGES (ARCH_MAX_RAM_KBS / ARCH_PAGE_SIZE * 1024) 
#define BITS_PER_ENTRY (sizeof(size_t) * 8)
#define BITMAP_ENTRIES (MAX_MEMORY_PAGES / BITS_PER_ENTRY)
static size_t allocation_bitmap[BITMAP_ENTRIES];

/*
 * Stores pages that are available for us to allocate. If set to NULL, then we
 * have yet to reinitialise the physical memory manager. The stack grows upward, 
 * and the pointer is incremented after writing the value on a push. The stack 
 * stores physical page numbers (indexes) instead of addresses.
 */
static size_t* allocation_stack = NULL;
static size_t allocation_stack_pointer = 0;

/*
 * Once we get below this number, we will start evicting pages.
 */
#define NUM_RESERVE_PAGES 32

/*
 * The number of physical pages available (free) remaining, and total, in the 
 * system.
 */
static size_t pages_left = 0;
static size_t total_pages = 0;

/*
 * The highest physical page number that exists on this system. Gets set during 
 * InitPhys() when scanning the system's memory map.
 */
static size_t highest_page_index = 0;

static inline bool IsBitmapEntryFree(size_t index) {
    size_t base = index / BITS_PER_ENTRY;
    size_t offset = index % BITS_PER_ENTRY;
    return allocation_bitmap[base] & (1 << offset);
}

static inline void AllocateBitmapEntry(size_t index) {
    assert(IsBitmapEntryFree(index));

    size_t base = index / BITS_PER_ENTRY;
    size_t offset = index % BITS_PER_ENTRY;
    allocation_bitmap[base] &= ~(1 << offset);
}

static inline void DeallocateBitmapEntry(size_t index) {
    assert(!IsBitmapEntryFree(index));

    size_t base = index / BITS_PER_ENTRY;
    size_t offset = index % BITS_PER_ENTRY;
    allocation_bitmap[base] |= 1 << offset;
}

static inline void PushIndex(size_t index) {
    assert(index <= highest_page_index);
    allocation_stack[allocation_stack_pointer++] = index;
}

static inline size_t PopIndex(void) {
    assert(allocation_stack_pointer != 0);
    return allocation_stack[--allocation_stack_pointer];
}

/*
 * Removes an entry from the stack by value. Only to be used when absolutely 
 * required, as it has O(n) runtime and is therefore very slow. 
 */
static void RemoveStackEntry(size_t index) {
    for (size_t i = 0; i < allocation_stack_pointer; ++i) {
        if (allocation_stack[i] == index) {
            memmove(
                allocation_stack + i, 
                allocation_stack + i + 1, 
                (--allocation_stack_pointer - i) * sizeof(size_t)
            );
            return;
        }
    }
}

/**
 * Deallocates a page of physical memory that was allocated with AllocPhys(). 
 * Does not affect virtual mappings - that should be taken care of before
 * deallocating. Address must be page aligned.
 */
void DeallocPhys(size_t addr) {
    MAX_IRQL(IRQL_SCHEDULER);
    assert(addr % ARCH_PAGE_SIZE == 0);

    size_t page = addr / ARCH_PAGE_SIZE;

    AcquireSpinlock(&phys_lock);

    ++pages_left;
    DeallocateBitmapEntry(page);
    if (allocation_stack != NULL) {
        PushIndex(page);
    }
    ReleaseSpinlock(&phys_lock);

    if (pages_left > NUM_RESERVE_PAGES * 2) {
        SetDiskCaches(DISKCACHE_NORMAL);
    }
}

/**
 * Deallocates a section of physical memory that was allocated with 
 * AllocPhysContinuous(). The entire block of memory must be deallocated at 
 * once, i.e. the start address of the memory should be passed in. Does not
 * affect virtual mappings - that should be taken care of before deallocating.
 * 
 * @param addr The address of the section of memory to deallocate. Must be
 *             page-aligned.
 * @param size The size of the allocation. This should be the same value that 
 *             was passed into AllocPhysContinuous().
 */
void DeallocPhysContiguous(size_t addr, size_t bytes) {
    for (size_t i = 0; i < BytesToPages(bytes); ++i) {
        DeallocPhys(addr);
        addr += ARCH_PAGE_SIZE;
    }
}

static void EvictPagesIfNeeded(void*) {
    EXACT_IRQL(IRQL_STANDARD);

    extern int handling_page_fault;
    if (handling_page_fault > 0) {
        return;
    }

    if (pages_left < NUM_RESERVE_PAGES) {
        SetDiskCaches(DISKCACHE_TOSS);

    } else if (pages_left < NUM_RESERVE_PAGES * 3 / 2) {
        SetDiskCaches(DISKCACHE_REDUCE);
    }

    int timeout = 0;
    while (pages_left < NUM_RESERVE_PAGES && timeout < 5) {
        handling_page_fault++;
        EvictVirt();
        handling_page_fault--;
        ++timeout;
    }
    LogWriteSerial("Done EvictPagesIfNeeded\n");
}

size_t AllocPhys(void) {
    MAX_IRQL(IRQL_SCHEDULER);

    AcquireSpinlock(&phys_lock);

    if (pages_left == 0) {
        Panic(PANIC_OUT_OF_PHYS);
    }
    if (pages_left <= NUM_RESERVE_PAGES) {
        DeferUntilIrql(IRQL_STANDARD, EvictPagesIfNeeded, NULL);
    }

    size_t index = 0;
    if (allocation_stack == NULL) {
        /*
         * No stack yet, so must use the bitmap. No point optimising this as
         * only used during boot.
         * 
         * Go backwards to keep low memory as free as possible for e.g. DMA
         */
        while (!IsBitmapEntryFree(index)) {
            index = (index + MAX_MEMORY_PAGES - 1) % MAX_MEMORY_PAGES;
        }
    } else {
        index = PopIndex();
    }

    AllocateBitmapEntry(index);
    --pages_left;
    ReleaseSpinlock(&phys_lock);

    return index * ARCH_PAGE_SIZE;
}

/**
 * Allocates a section of contigous physical memory, that may or may not have 
 * requirements as to where the memory can be located. Will not cause pages to 
 * be evicted from RAM to 'make room' so sufficient memory must already exit.
 *
 * If there is no minimum, maximum, or boundary, specifiy these as zero.
 */
size_t AllocPhysContiguous(
    size_t bytes, size_t min_addr, size_t max_addr, size_t boundary
) {
    if (allocation_stack == NULL) {
        return 0;
    }

    size_t pages = BytesToPages(bytes);
    size_t min_index = (min_addr + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
    size_t max_index = max_addr == 0 ? highest_page_index + 1 : max_addr / ARCH_PAGE_SIZE;
    size_t count = 0;

    AcquireSpinlock(&phys_lock);

    if (pages + NUM_RESERVE_PAGES >= pages_left) {
        ReleaseSpinlock(&phys_lock);
        return 0;
    }

    for (size_t index = min_index; index < max_index; ++index) {
        bool crossed_boundary = (boundary != 0 && (index % (boundary / ARCH_PAGE_SIZE) == 0));
        if (!IsBitmapEntryFree(index) || crossed_boundary) {
            count = 0;
            continue;
        }

        ++count;
        if (count == pages) {
            size_t start_index = index - count + 1;
            while (start_index <= index) {
                AllocateBitmapEntry(start_index);
                RemoveStackEntry(start_index);
                ++start_index;
            }

            ReleaseSpinlock(&phys_lock);
            return start_index * ARCH_PAGE_SIZE;
        }
    }

    ReleaseSpinlock(&phys_lock);
    return 0;
}

/**
 * Initialises the physical memory manager for the first time. Must be called 
 * before any other memory management function is called. It determines what 
 * memory is available on the system  and prepares the O(n) bitmap allocator. 
 * This will be slow, but is only needed until ReinitPhys() gets called.
 */
void InitPhys(struct kernel_boot_info* boot_info) {
    InitSpinlock(&phys_lock, "phys", IRQL_SCHEDULER);

	while (true) {
		struct boot_memory_entry* range = ArchGetMemory(boot_info);

		if (range == NULL) {
			break;

		} else {
			/* 
			* Must round the start address up so we don't include memory outside
            * the region.
            */
			size_t first_page = (range->address + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
			size_t last_page = (range->address + range->length) / ARCH_PAGE_SIZE;

			while (first_page < last_page && first_page < MAX_MEMORY_PAGES) {
                DeallocateBitmapEntry(first_page);
                ++pages_left;
                ++total_pages;

                if (first_page > highest_page_index) {
                    highest_page_index = first_page;
                }

                ++first_page;
			}
		}
	}

    MarkTfwStartPoint(TFW_SP_AFTER_PHYS);
}

/*
 * Save extra physical memory on by deallocating the memory in the bitmap that 
 * can't be reached (due to the system not having that much memory).
 */
static void ReclaimBitmapSpace(void) {
    size_t unreachable_pages = MAX_MEMORY_PAGES - (highest_page_index + 1);
    size_t unreachable_bitmap_pages = (unreachable_pages / BITS_PER_ENTRY) / ARCH_PAGE_SIZE;
    size_t end_bitmap = ((size_t) allocation_bitmap) + sizeof(allocation_bitmap);
    
    /*
     * Round down, otherwise other kernel data in the same page as the end of 
     * the bitmap  will also be counted as 'free', causing memory corruption.
     */
    size_t unreachable_region = ((end_bitmap - ARCH_PAGE_SIZE * unreachable_bitmap_pages));
    unreachable_region &= ~(ARCH_PAGE_SIZE - 1);

    while (unreachable_bitmap_pages--) {
        DeallocPhys(ArchVirtualToPhysical(unreachable_region));
        unreachable_region += ARCH_PAGE_SIZE;
        ++total_pages;
    }
}

/**
 * Reinitialises the physical memory manager with a constant-time page 
 * allocation system. Must be called after virtual memory has been initialised. 
 */
void ReinitPhys(void) {
    assert(allocation_stack == NULL);

    allocation_stack = (size_t*) MapVirt(
        0, 0, (highest_page_index + 1) * sizeof(size_t), 
        VM_READ | VM_WRITE | VM_LOCK, NULL, 0
    );
    
    for (size_t i = 0; i < MAX_MEMORY_PAGES; ++i) {
        if (IsBitmapEntryFree(i)) {
            PushIndex(i);
        }
    }

    ReclaimBitmapSpace();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS_REINIT);
}

size_t GetTotalPhysKilobytes(void) {
    return total_pages * (ARCH_PAGE_SIZE / 1024);
}

size_t GetFreePhysKilobytes(void) {
    return pages_left * (ARCH_PAGE_SIZE / 1024);
}