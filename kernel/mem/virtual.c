
#include <virtual.h>
#include <avl.h>
#include <heap.h>
#include <common.h>
#include <arch.h>
#include <physical.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <spinlock.h>
#include <log.h>
#include <sys/types.h>
#include <irql.h>
#include <cpu.h>
#include <transfer.h>
#include <swapfile.h>
#include <vfs.h>
#include <errno.h>

static struct vas_entry* GetVirtEntry(struct vas* vas, size_t virtual);

/**
 * Stores a pointer to any kernel VAS. Ensures that when processes are destroyed, we are using a VAS
 * that is different from the VAS that's being deleted. 
 */
static struct vas* kernel_vas;

// TODO: lots of locks! especially the global cpu one

void AvlPrinter(void* data_) {
    struct vas_entry* data = (struct vas_entry*) data_;
    LogWriteSerial("[v: 0x%X, p: 0x%X; acrl: %d%d%d%d. ref: %d]; ", data->virtual, data->physical, data->allocated, data->cow, data->in_ram, data->lock, data->ref_count);
}

/**
 * Whether or not virtual memory is available for use. Can be read with IsVirtInitialised(), and is set when
 * InitVirt() has completed.
 * 
 * Does not have a lock, as only the bootstrap CPU should be modifiying it, and this happens before threads
 * are set up. Once set to true, it is never changed again, so there is no read/write problems.
 */
static bool virt_initialised = false;

static int VirtAvlComparator(void* a, void* b) {
    struct vas_entry* a_entry = (struct vas_entry*) a;
    struct vas_entry* b_entry = (struct vas_entry*) b;
    if (a_entry->virtual == b_entry->virtual) {
        return 0;
    }
    return (a_entry->virtual < b_entry->virtual) ? -1 : 1;
}

/**
 * Initialises a virtual address space in an already allocated section of memory.
 * 
 * @param vas The memory to initialise a virtual address space object in.
 * @param flags Can be 0 or VAS_NO_ARCH_INIT. If VAS_NO_ARCH_INIT is provided, then no architecture-specific
 *              code will be called. This flag should only be set if called by architecture-specific functions
 *              (e.g. to create the initial address space). Normally, 0 should be passed in.
 * 
 * @maxirql IRQL_SCHEDULER
 */
void CreateVasEx(struct vas* vas, int flags) {
    MAX_IRQL(IRQL_SCHEDULER);

    vas->mappings = AvlTreeCreate();

    /*
     * We are in for a world of hurt if someone is able to page fault while
     * holding the lock on a virtual address space, so better make it IRQL_SCHEDULER.
     */
    InitSpinlock(&vas->lock, "vas", IRQL_SCHEDULER);
    AvlTreeSetComparator(vas->mappings, VirtAvlComparator);
    if (!(flags & VAS_NO_ARCH_INIT)) {
        ArchInitVas(vas);
    }
}

/**
 * Allocates and initialises a new virtual address space.
 * 
 * @return The virtual address space which was created.
 * 
 * @maxirql IRQL_SCHEDULER
 */
struct vas* CreateVas() {
    MAX_IRQL(IRQL_SCHEDULER);

    struct vas* vas = AllocHeap(sizeof(struct vas));
    CreateVasEx(vas, 0);
    return vas;
}

struct defer_disk_access {
    struct open_file* file;
    off_t offset;
    size_t address;
    int direction;
};

static void PerformDeferredAccess(void* data) {
    struct defer_disk_access* access = (struct defer_disk_access*) data;
    struct transfer tr = CreateKernelTransfer((void*) access->address, ARCH_PAGE_SIZE, access->offset, access->direction);
    
    bool write = access->direction == TRANSFER_WRITE;
    LogWriteSerial("ABOUT TO ACCESS DISK DEFERRED... DIR %d\n", access->direction);
    int res = (write ? WriteFile : ReadFile)(access->file, &tr);
    if (res != 0) {
        Panic(PANIC_DISK_FAILURE_ON_SWAPFILE);
    }

    if (write) {
        UnmapVirt(access->address, ARCH_PAGE_SIZE);
    } else {

        size_t* oadr = (size_t*) access->address;
        LogWriteSerial("\nDEFER READ to 0x%X [0x%X]:\n", oadr, access->offset);
        for (size_t i = 0; i < ARCH_PAGE_SIZE / sizeof(size_t); ++i) {
            LogWriteSerial("0x%X, ", *oadr++);
        }
        LogWriteSerial("\n\n");

        DeallocateSwapfileIndex(access->offset / ARCH_PAGE_SIZE);
        UnlockVirt(access->address);
    }
    LogWriteSerial("DISK ACCESS DONE FOR SWAPFILE - ALL GOOD 0x%X (DIR %d).\n", access->offset, access->direction);

    FreeHeap(access);
}

/**
 * Given a virtual page, it defers a write to disk. It creates a copy of the virtual page, so that it may be safely
 * deleted as soon as this gets called.
 */
static void DeferDiskWrite(size_t old_addr, struct open_file* file, off_t offset) {
    size_t new_addr = MapVirt(0, 0, ARCH_PAGE_SIZE, VM_LOCK | VM_READ | VM_WRITE | VM_RECURSIVE, NULL, 0);
    inline_memcpy((void*) new_addr, (const char*) old_addr, ARCH_PAGE_SIZE);

    size_t* oadr = (size_t*) old_addr;
    LogWriteSerial("\nDEFER WRITE [0x%X]:\n", offset);
    for (size_t i = 0; i < ARCH_PAGE_SIZE / sizeof(size_t); ++i) {
        LogWriteSerial("0x%X, ", *oadr++);
    }
    LogWriteSerial("\n\n");

    struct defer_disk_access* access = AllocHeap(sizeof(struct defer_disk_access));
    access->address = new_addr;
    access->file = file;
    access->direction = TRANSFER_WRITE;
    access->offset = offset;
    DeferUntilIrql(IRQL_PAGE_FAULT, PerformDeferredAccess, (void*) access);
}

static void DeferDiskRead(size_t new_addr, struct open_file* file, off_t offset) {
    LockVirtEx(GetVas(), new_addr);

    struct defer_disk_access* access = AllocHeap(sizeof(struct defer_disk_access));
    access->address = new_addr;
    access->file = file;
    access->direction = TRANSFER_READ;
    access->offset = offset;
    DeferUntilIrql(IRQL_PAGE_FAULT, PerformDeferredAccess, (void*) access);
}

/**
 * Evicts a particular page mapping from virtual memory, freeing up its physical page (if it had one).
 * This will often involve accessing the disk to put it on swapfile (or save modifications to a file-backed
 * page).
 * 
 * @param vas The virtual address space that we're evicting from. Does not have to be the current one.
 * @param entry The virtual page to remove from virtual memory.
 * 
 * @maxirql IRQL_SCHEDULER
 */
void EvictPage(struct vas* vas, struct vas_entry* entry) {
    MAX_IRQL(IRQL_STANDARD);

    assert(!entry->lock);
    assert(!entry->cow);

    AcquireSpinlockIrql(&vas->lock);

    if (!entry->in_ram) {
        /*
         * Nothing happens, as this page isn't even in RAM.
         */
         
    } else if (entry->file) {
        /*
         * We will just reload it from disk next time.
         */

        if (entry->write) {
            DeferDiskWrite(entry->virtual, entry->file_node, entry->file_offset);
        }

        entry->in_ram = false;
        entry->allocated = false;
        DeallocPhys(entry->physical);
        ArchUnmap(vas, entry);
        ArchFlushTlb(vas);

    } else {
        /*
         * Otherwise, we need to mark it as swapfile.
         */
        entry->in_ram = false;
        entry->swapfile = true;
        entry->allocated = false;
        
        uint64_t offset = AllocateSwapfileIndex() * ARCH_PAGE_SIZE;
        LogWriteSerial(" ----> WRITING VIRT 0x%X TO SWAP: DISK INDEX 0x%X (offset 0x%X)\n", entry->virtual, (int) offset / ARCH_PAGE_SIZE, (int) offset);
        DeferDiskWrite(entry->virtual, GetSwapfile(), offset);
        entry->swapfile_offset = offset;

        ArchUnmap(vas, entry);
        DeallocPhys(entry->physical);
        ArchFlushTlb(vas);
    }

    ReleaseSpinlockIrql(&vas->lock);
}

/*
 * Lower value means it should be swapped out first.
 */
static int GetPageEvictionRank(struct vas* vas, struct vas_entry* entry) {
    (void) vas;
    
    /*
     * Want to evict in this order:
     *      - file and non-writable
     *      - file and writable
     *      - non-writable
     *      - writable
     * 
     * When we have a way of dealing with accessed / dirty, it should be in this order:
     * 
     *   0 FILE, NON-WRITABLE, NON-ACCESSED
     *  10 FILE, WRITABLE, NON-DIRTY, NON-ACCESSED
     *  20 FILE, NON-WRITABLE, ACCESSED
     *  30 FILE, WRITABLE, NON-DIRTY, ACCESSED 
     *  40 NORMAL, NON-DIRTY, NON-ACCESSED
     *  50 NORMAL, NON-DIRTY, ACCESSED
     *  60 FILE, WRITABLE, DIRTY, <don't care>
     *  70 NORMAL, DIRTY, <don't care>
     *  80 COW, 
     * 
     *  Globals add 3 points.
     */

    bool accessed;
    bool dirty;
    ArchGetPageUsageBits(vas, entry, &accessed, &dirty);
    ArchSetPageUsageBits(vas, entry, false, false);

    int penalty = (entry->global ? 3 : 0) + entry->times_swapped * 8;

    if (entry->cow) {
        return 80 + penalty;

    } else if (entry->file && !entry->write) {
        return (accessed ? 20 : 0) + penalty;

    } else if (entry->file && entry->write) {
        return (dirty ? 60 : (accessed ? 30 : 10)) + penalty;
    
    } else if (!dirty) {
        return (accessed ? 50 : 40) + penalty;

    } else {
        return 70 + penalty;
    }
}

struct eviction_candidate {
    struct vas* vas;
    struct vas_entry* entry;
};

void FindVirtToEvictFromSubtree(struct vas* vas, struct avl_node* node, int* lowest_rank, struct eviction_candidate* lowest_ranked, int* count, struct vas_entry** prev_swaps) {
    static uint8_t rand = 0;
    
    if (node == NULL) {
        return;
    }

    if (*lowest_rank < 10) {
        /*
        * No need to look anymore - we've already a best possible page.
        */
       return;
    }

    *count += 1;

    /*
     * After scanning through 500 entries, we'll allow early exits for less optimal pages.
     */
    int limit = (((*count - 500) / 75) + 10);
    if (*count > 500 && *lowest_rank < limit) {
        return;
    }

    struct vas_entry* entry = AvlTreeGetData(node);
    if (!entry->lock && entry->allocated) {
        int rank = GetPageEvictionRank(vas, entry);

        /*
         * To ensure we mix up who gets evicted, when there's an equality, we use it 1/4 times.
         * It is likely there are more than 4 to replace, so this ensures that we cycle through many of them.
         */
        bool equal = rank == *lowest_rank;
        if (equal) {
            equal = (rand++ & 3) == 0;
        }

        bool prev_swap = false;
        for (int i = 0; i < 8; ++i) {
            if (prev_swaps[i] == entry) {
                prev_swap = true;
                break;
            }
        }

        if ((rank < *lowest_rank || equal) && !prev_swap) {
            lowest_ranked->vas = vas;
            lowest_ranked->entry = entry;
            *lowest_rank = rank;

            if (rank == 0) {
                return;
            }
        }
    }

    FindVirtToEvictFromSubtree(vas, AvlTreeGetLeft(node), lowest_rank, lowest_ranked, count, prev_swaps);
    FindVirtToEvictFromSubtree(vas, AvlTreeGetRight(node), lowest_rank, lowest_ranked, count, prev_swaps);
}

void FindVirtToEvictFromAddressSpace(struct vas* vas, int* lowest_rank, struct eviction_candidate* lowest_ranked, bool include_globals, struct vas_entry** prev_swaps) {
    int count = 0;
    FindVirtToEvictFromSubtree(vas, AvlTreeGetRootNode(vas->mappings), lowest_rank, lowest_ranked, &count, prev_swaps);
    if (include_globals) {
        FindVirtToEvictFromSubtree(vas, AvlTreeGetRootNode(GetCpu()->global_vas_mappings), lowest_rank, lowest_ranked, &count, prev_swaps);
    }
}

/**
 * Searches through virtual memory (that doesn't necessarily have to be in the current virtual address space),
 * and finds and evicts a page of virtual memory, to try free up physical memory. 
 * 
 * @maxirql IRQL_STANDARD
 */
void EvictVirt(void) {
    MAX_IRQL(IRQL_STANDARD);

    if (GetSwapfile() == NULL) {
        return;
    }

    // don't allow any of the last 8 swaps to be repeated (as an instruction may require at least 6 pages on x86
    // if it straddles many boundaries)
    static struct vas_entry* previous_swaps[8] = {0};
    static int swap_num = 0;

    int lowest_rank = 10000;
    struct eviction_candidate lowest_ranked;
    lowest_ranked.entry = NULL;
    
    AcquireSpinlockIrql(&GetVas()->lock);
    FindVirtToEvictFromAddressSpace(GetVas(), &lowest_rank, &lowest_ranked, true, previous_swaps);
    ReleaseSpinlockIrql(&GetVas()->lock);

    // TODO: go through other address spaces

    while (false) {
        struct vas* vas = NULL;
        if (vas != GetVas()) {
            FindVirtToEvictFromAddressSpace(GetVas(), &lowest_rank, &lowest_ranked, true, previous_swaps);
        }
    }

    if (lowest_ranked.entry != NULL) {
        previous_swaps[swap_num++ % 8] = lowest_ranked.entry;
        EvictPage(lowest_ranked.vas, lowest_ranked.entry);
        lowest_ranked.entry->times_swapped++;
    }
}

static void InsertIntoAvl(struct vas* vas, struct vas_entry* entry) {
    assert(IsSpinlockHeld(&vas->lock));
    
    if (entry->global) { 
        AcquireSpinlockIrql(&GetCpu()->global_mappings_lock);
        AvlTreeInsert(GetCpu()->global_vas_mappings, entry);
        ReleaseSpinlockIrql(&GetCpu()->global_mappings_lock);

    } else {        
        AvlTreeInsert(vas->mappings, entry);
    }
}

static void DeleteFromAvl(struct vas* vas, struct vas_entry* entry) {
    assert(IsSpinlockHeld(&vas->lock));
    if (entry->global) {
        AcquireSpinlockIrql(&GetCpu()->global_mappings_lock);
        AvlTreeDelete(GetCpu()->global_vas_mappings, entry);
        ReleaseSpinlockIrql(&GetCpu()->global_mappings_lock);

    } else {
        AvlTreeDelete(vas->mappings, entry); 
    }
}

/**
 * Adds a virtual page mapping to the specified virtual address space. This will add it both to the mapping tree
 * and the architectural paging structures (so that page faults can be raised, etc., if there is no backing yet).
 * 
 * @param vas       The virtual address space to map this page to
 * @param physical  Only used if VM_LOCK is specified in flags. Determines the physical page that will back the
 *                      virtual mapping. If VM_LOCK is set, and this is 0, then a physical page will be allocated. If
 *                      VM_LOCK is set, and this is non-zero, then that physical address will be used.
 * @param virtual   The virtual address to map the memory to. This should be non-zero.
 * @param flags     Various bitflags to affect the attributes of the mapping. Flags that are used here are:
 *                      VM_READ     : if set, the page will be marked as readable
 *                      VM_WRITE    : if set, the page will be marked as writable
 *                      VM_USER     : if set, then usermode can access this page without faulting
 *                      VM_EXEC     : if set, then code can be executed in this page
 *                      VM_LOCK     : if set, the page will immediately get a physical memory backing, and will not be
 *                                    paged out
 *                      VM_FILE     : if set, this page is file-backed
 *                  All other flags are ignored by this function.
 * @param file      If VM_FILE is set, then the page is backed by this file, starting at the position specified by pos.
 * @param pos       If VM_FILE is set, then this is the offset into the file where the page is mapped to.
 * 
 * @maxirql IRQL_SCHEDULER
 */
static void AddMapping(struct vas* vas, size_t physical, size_t virtual, int flags, struct open_file* file, off_t pos) {
    MAX_IRQL(IRQL_SCHEDULER);

    assert(!(file != NULL && (flags & VM_FILE) == 0));

    struct vas_entry* entry = AllocHeapZero(sizeof(struct vas_entry));
    entry->allocated = false;

    bool lock = flags & VM_LOCK;
    entry->lock = lock;
    entry->in_ram = lock;

    if (lock) {
        /*
         * We are not allowed to check if the physical page is allocated/free, because it might come
         * from a VM_MAP_HARDWARE request, which can map non-RAM pages. 
         */
        if (physical == 0) {
            physical = AllocPhys();
            entry->allocated = true;
        }
    }
    
    /*
     * MapVirt checks for conflicting flags and returns, so this code doesn't need to worry about that.
     */
    entry->virtual = virtual;
    entry->times_swapped = 0;
    entry->read = flags & VM_READ;
    entry->write = flags & VM_WRITE;
    entry->exec = flags & VM_EXEC;
    entry->file = (flags & VM_FILE) ? 1 : 0;
    entry->user = flags & VM_USER;
    entry->global = !(flags & VM_LOCAL);
    entry->physical = physical;
    entry->ref_count = 1;
    entry->file_offset = pos;
    entry->file_node = file;
    entry->swapfile = false;
    entry->swapfile_offset = 0xDEADDEAD;

    LogWriteSerial("Adding mapping at 0x%X. flags = 0x%X, rwxgu'lfia = %d%d%d%d%d'%d%d%d%d. p 0x%X\n", 
        entry->virtual, flags,
        entry->read, entry->write, entry->exec, entry->global, entry->user, entry->lock, entry->file, entry->in_ram, entry->allocated,
        entry->physical 
    );

    /*
     * TODO: later on, check if shared, and add phys->virt entry if needed
     */
    
    if ((flags & VM_RECURSIVE) == 0) {
        AcquireSpinlockIrql(&vas->lock);
    }
    InsertIntoAvl(vas, entry);
    ArchAddMapping(vas, entry);
    if ((flags & VM_RECURSIVE) == 0) {
        ReleaseSpinlockIrql(&vas->lock);
    }
}

static bool IsRangeInUse(struct vas* vas, size_t virtual, size_t pages) {
    bool in_use = false;

    struct vas_entry dummy;
    dummy.virtual = virtual;

    /*
     * We have to loop over the local one, and if it isn't there, the global one. We do this
     * in separate loops to prevent the need to acquire both spinlocks at once, which could lead
     * to a deadlock.
     */

    AcquireSpinlockIrql(&vas->lock);
    for (size_t i = 0; i < pages; ++i) {
        if (AvlTreeContains(vas->mappings, (void*) &dummy)) {
            in_use = true;
            break;
        }
        dummy.virtual += ARCH_PAGE_SIZE;
    }
    ReleaseSpinlockIrql(&vas->lock);

    if (in_use) {
        return true;
    }

    AcquireSpinlockIrql(&GetCpu()->global_mappings_lock);
    dummy.virtual = virtual;
    for (size_t i = 0; i < pages; ++i) {
        if (AvlTreeContains(GetCpu()->global_vas_mappings, (void*) &dummy)) {
            in_use = true;
            break;
        }
        dummy.virtual += ARCH_PAGE_SIZE;
    }
    ReleaseSpinlockIrql(&GetCpu()->global_mappings_lock);

    return in_use;
}

static size_t AllocVirtRange(struct vas* vas, size_t pages, int flags) {
    /*
     * TODO: make this deallocatable, and not x86 specific (with that memory address)
     */
    if (flags & VM_LOCAL) {
        /*
         * Also needs to use the vas to work out what's allocated in that vas
         */
        (void) vas;
        static size_t hideous_allocator = 0x20000000U;
        size_t retv = hideous_allocator;
        hideous_allocator += pages * ARCH_PAGE_SIZE;
        return retv;

    } else {
        /*
         * TODO: this probably needs a global lock of some sort.
         */
        static size_t hideous_allocator = ARCH_KRNL_SBRK_BASE;
        size_t retv = hideous_allocator;
        hideous_allocator += pages * ARCH_PAGE_SIZE;
        return retv;
    }
}

static void FreeVirtRange(struct vas* vas, size_t virtual, size_t pages) {
    (void) virtual;
    (void) vas;
    (void) pages;
}

/**
 * Creates a virtual memory mapping.
 * 
 * @param vas       The virtual address space to map this page to
 * @param physical  Only used if VM_LOCK is specified in flags. Determines the physical page that will back the
 *                      virtual mapping. If VM_LOCK is set, and this is 0, then a physical page will be allocated. If
 *                      VM_LOCK is set, and this is non-zero, then that physical address will be used. In this instance,
 *                      VM_MAP_HARDWARE must also be set. If VM_MAP_HARDWARE is not set, this value must be 0.
 * @param virtual   The virtual address to map the memory to. If this is 0, then a virtual memory region of the correct
 *                      size will be allocated.
 * @param pages     The number of contiguous pages to map in this way
 * @param flags     Various bitflags to affect the attributes of the mapping. Flags that are used here are:
 *                      VM_READ         : if set, the page will be marked as readable
 *                      VM_WRITE        : if set, the page will be marked as writable
 *                      VM_USER         : if set, then usermode can access this page without faulting
 *                      VM_EXEC         : if set, then code can be executed in this page
 *                      VM_LOCK         : if set, the page will immediately get a physical memory backing, and will not be
 *                                        paged out
 *                      VM_FILE         : if set, this page is file-backed. Cannot be combined with VM_MAP_HARDWARE.
 *                      VM_MAP_HARDWARE : if set, a physical address can be specified for the backing. If this flag is set,
 *                                        then VM_LOCK must also be set, and VM_FILE must be clear.
 * @param file      If VM_FILE is set, then the page is backed by this file, starting at the position specified by pos.
 *                      If VM_FILE is clear, then this value must be NULL.
 * @param pos       If VM_FILE is set, then this is the offset into the file where the page is mapped to. If VM_FILE is clear,
 *                      then this value must be 0.
 * 
 * @maxirql IRQL_SCHEDULER
 */
static size_t MapVirtEx(struct vas* vas, size_t physical, size_t virtual, size_t pages, int flags, struct open_file* file, off_t pos) {
    MAX_IRQL(IRQL_SCHEDULER);

    /*
     * We only specify a physical page when we need to map hardware directly (i.e. it's not
     * part of the available RAM the physical memory manager can give).
     */
    if (physical != 0 && (flags & VM_MAP_HARDWARE) == 0) {
        return 0;
    }

    if ((flags & VM_MAP_HARDWARE) && (flags & VM_FILE)) {
        return 0;
    }

    if ((flags & VM_MAP_HARDWARE) && (flags & VM_LOCK) == 0) {
        return 0;
    }

    if ((flags & VM_FILE) && file == NULL) {
        return 0;
    }
    
    if ((flags & VM_FILE) == 0 && (file != NULL || pos != 0)) {
        return 0;
    }

    if ((flags & VM_FILE) && (flags & VM_LOCK)) {
        LogWriteSerial("someone tried to alloced VM_FILE and VM_LOCK at the same time!\n");
        return 0;
    }

    /*
     * Get a virtual memory range that is not currently in use.
     */
    if (virtual == 0) {
        virtual = AllocVirtRange(vas, pages, flags & VM_LOCAL);

    } else {
        // TODO: need to lock here to make the israngeinuse and allocvirtrange to be atomic
        if (IsRangeInUse(vas, virtual, pages)) {
            if (flags & VM_FIXED_VIRT) {
                return 0;
            }

            virtual = AllocVirtRange(vas, pages, flags & VM_LOCAL);
        }
    }

    for (size_t i = 0; i < pages; ++i) {
        AddMapping(vas, physical == 0 ? 0 : (physical + i * ARCH_PAGE_SIZE), virtual + i * ARCH_PAGE_SIZE, flags, file, pos + i * ARCH_PAGE_SIZE);
    }   

    if (vas == GetVas()) {
        ArchFlushTlb(vas);
    }
    
    return virtual;
}

/**
 * Creates a virtual memory mapping in the current virtual address space.
 * 
 * @param physical  See `MapVirtEx`
 * @param virtual   See `MapVirtEx`
 * @param pages     The minimum number of bytes to map
 * @param flags     See `MapVirtEx`
 * @param file      See `MapVirtEx`
 * @param pos       See `MapVirtEx`
 * 
 * @maxirql IRQL_SCHEDULER
 */
size_t MapVirt(size_t physical, size_t virtual, size_t bytes, int flags, struct open_file* file, off_t pos) {
    MAX_IRQL(IRQL_SCHEDULER);

    size_t pages = (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
    return MapVirtEx(GetVas(), physical, virtual, pages, flags, file, pos);
}

static struct vas_entry* GetVirtEntry(struct vas* vas, size_t virtual) {
    struct vas_entry dummy;
#ifndef NDEBUG   
    dummy.physical = 0xBAADC0DE;
#endif
    dummy.virtual = virtual & ~(ARCH_PAGE_SIZE - 1);

    assert(IsSpinlockHeld(&vas->lock));

    struct vas_entry* res = (struct vas_entry*) AvlTreeGet(vas->mappings, (void*) &dummy);
    if (res == NULL) {
        AcquireSpinlockIrql(&GetCpu()->global_mappings_lock);
        res = (struct vas_entry*) AvlTreeGet(GetCpu()->global_vas_mappings, (void*) &dummy);
        ReleaseSpinlockIrql(&GetCpu()->global_mappings_lock);
        assert(res != NULL);
    }
    return res;
}

size_t GetPhysFromVirt(size_t virtual) {
    struct vas* vas = GetVas();
    AcquireSpinlockIrql(&vas->lock);
    size_t result = GetVirtEntry(GetVas(), virtual)->physical;
    ReleaseSpinlockIrql(&vas->lock);
    return result;
}

static void BringIntoMemoryFromCow(struct vas_entry* entry) {
    /*
    * If someone deallocates a COW page in another process to get the ref
    * count back to 1 already, then we just have the page to ourselves again.
    */
    if (entry->ref_count == 1) {
        entry->cow = false;
        ArchUpdateMapping(GetVas(), entry);
        ArchFlushTlb(GetVas());
        return;
    }

    uint8_t page_data[ARCH_PAGE_SIZE];
    inline_memcpy(page_data, (void*) entry->virtual, ARCH_PAGE_SIZE);

    entry->ref_count--;

    if (entry->ref_count == 1) {
        entry->cow = false;
    }
        
    struct vas_entry* new_entry = AllocHeap(sizeof(struct vas_entry));
    *new_entry = *entry;
    new_entry->ref_count = 1;
    new_entry->physical = AllocPhys();
    new_entry->allocated = true;
    DeleteFromAvl(GetVas(), entry);
    FreeHeap(entry);
    ArchUpdateMapping(GetVas(), entry);
    ArchFlushTlb(GetVas());
    inline_memcpy((void*) entry->virtual, page_data, ARCH_PAGE_SIZE);
}

static void BringIntoMemoryFromFile(struct vas_entry* entry) {
    entry->physical = AllocPhys();
    entry->allocated = true;
    entry->in_ram = true;
    entry->swapfile = false;
    ArchUpdateMapping(GetVas(), entry);
    ArchFlushTlb(GetVas());

    DeferDiskRead(entry->virtual, entry->file_node, entry->file_offset);
}

static void BringIntoMemoryFromSwapfile(struct vas_entry* entry) {
    assert(!entry->file);
    uint64_t offset = entry->swapfile_offset;
    entry->physical = AllocPhys();
    entry->allocated = true;
    entry->in_ram = true;
    entry->swapfile = false;
    ArchUpdateMapping(GetVas(), entry);
    ArchFlushTlb(GetVas());

    LogWriteSerial(" ----> RELOADING SWAP TO VIRT 0x%X: DISK INDEX 0x%X (offset 0x%X)\n", entry->virtual, (int) offset / ARCH_PAGE_SIZE, (int) offset);

    DeferDiskRead(entry->virtual, GetSwapfile(), offset);
}

static int BringIntoMemory(struct vas* vas, struct vas_entry* entry, bool allow_cow) {
    (void) vas;
    assert(IsSpinlockHeld(&vas->lock));

    if (entry->cow && allow_cow) {
        BringIntoMemoryFromCow(entry);
        return 0;
    }

    if (entry->file && !entry->in_ram) {
        BringIntoMemoryFromFile(entry);
        return 0;
    }

    if (entry->swapfile) {
        BringIntoMemoryFromSwapfile(entry);
        return 0;
    }

    // TODO: otherwise, as an entry exists, it just needs to be allocated-on-read/write (the default) and cleared to zero
    //       (it's not a non-paged area, as an entry for it exists)
    if (!entry->in_ram) {
        entry->physical = AllocPhys();
        entry->allocated = true;
        entry->in_ram = true;
        assert(!entry->swapfile);
        ArchUpdateMapping(GetVas(), entry);
        ArchFlushTlb(GetVas());
        inline_memset((void*) entry->virtual, 0, ARCH_PAGE_SIZE);
        return 0;
    }

    return EINVAL;
}

void LockVirtEx(struct vas* vas, size_t virtual) {
    struct vas_entry* entry = GetVirtEntry(vas, virtual);

    if (!entry->in_ram) {
        int res = BringIntoMemory(vas, entry, true);
        if (res != 0) {
            Panic(PANIC_CANNOT_LOCK_MEMORY);
        }
        assert(entry->in_ram);
    }

    entry->lock = true;
}

void UnlockVirtEx(struct vas* vas, size_t virtual) {
    struct vas_entry* entry = GetVirtEntry(vas, virtual);
    entry->lock = false;
}

void LockVirt(size_t virtual) {
    struct vas* vas = GetVas();
    AcquireSpinlockIrql(&vas->lock);
    LockVirtEx(vas, virtual);
    ReleaseSpinlockIrql(&vas->lock);
}

void UnlockVirt(size_t virtual) {
    struct vas* vas = GetVas();
    AcquireSpinlockIrql(&vas->lock);
    UnlockVirtEx(vas, virtual);
    ReleaseSpinlockIrql(&vas->lock);
}

void SetVirtPermissions(size_t virtual, int set, int clear) {
    /*
     * Only allow these flags to be set / cleared.
     */
    if ((set | clear) & ~(VM_READ | VM_WRITE | VM_EXEC)) {
        assert(false);
        return;
    }
    
    struct vas* vas = GetVas();
    AcquireSpinlockIrql(&vas->lock);

    struct vas_entry* entry = GetVirtEntry(vas, virtual);
    entry->read = (set & VM_READ) ? true : (clear & VM_READ ? false : entry->read);
    entry->write = (set & VM_WRITE) ? true : (clear & VM_WRITE ? false : entry->write);
    entry->exec = (set & VM_EXEC) ? true : (clear & VM_EXEC ? false : entry->exec);
    entry->user = (set & VM_USER) ? true : (clear & VM_USER ? false : entry->user);

    ArchUpdateMapping(vas, entry);
    ArchFlushTlb(vas);

    ReleaseSpinlockIrql(&vas->lock);
}

int GetVirtPermissions(size_t virtual) {
    struct vas* vas = GetVas();
    AcquireSpinlockIrql(&vas->lock);
    struct vas_entry* entry_ptr = GetVirtEntry(GetVas(), virtual);
    if (entry_ptr == NULL) {
        ReleaseSpinlockIrql(&vas->lock);
        return 0;
    }
    struct vas_entry entry = *entry_ptr;
    ReleaseSpinlockIrql(&vas->lock);

    int permissions = 0;
    if (entry.read) permissions |= VM_READ;
    if (entry.write) permissions |= VM_WRITE;
    if (entry.exec) permissions |= VM_EXEC;
    if (entry.lock) permissions |= VM_LOCK;
    if (entry.file) permissions |= VM_FILE;
    if (entry.user) permissions |= VM_USER;

    return permissions;
}

void UnmapVirtEx(struct vas* vas, size_t virtual, size_t pages) {
    bool needs_tlb_flush = false;

    for (size_t i = 0; i < pages; ++i) {
        struct vas_entry* entry = GetVirtEntry(vas, virtual + i * ARCH_PAGE_SIZE);
        entry->ref_count--;

        if (entry->ref_count == 0) {
            LogWriteSerial("Removing mapping at 0x%X\n", virtual + i * ARCH_PAGE_SIZE);

            if (entry->in_ram) {
                ArchUnmap(vas, entry);
                needs_tlb_flush = true;
            }
            if (entry->swapfile) {
                assert(!entry->allocated);
                DeallocateSwapfileIndex(entry->physical / ARCH_PAGE_SIZE);
            }
            if (entry->file && entry->write) { 
                DeferDiskWrite(entry->virtual, entry->file_node, entry->file_offset);
            }
            if (entry->allocated) {
                assert(!entry->swapfile);   // can't be on swap, as putting on swap clears allocated bit
                DeallocPhys(entry->physical);
            }

            DeleteFromAvl(vas, entry);
            FreeHeap(entry);
            FreeVirtRange(vas, virtual + i * ARCH_PAGE_SIZE, 1);
        }
    }

    if (needs_tlb_flush) {
        ArchFlushTlb(vas);
    }
}

void UnmapVirt(size_t virtual, size_t bytes) {
    size_t pages = (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
    struct vas* vas = GetVas();

    AcquireSpinlockIrql(&vas->lock);
    UnmapVirtEx(vas, virtual, pages);
    ReleaseSpinlockIrql(&vas->lock);
}

static void CopyVasRecursive(struct avl_node* node, struct vas* new_vas) {
    if (node == NULL) {
        return;
    }

    CopyVasRecursive(AvlTreeGetLeft(node), new_vas);
    CopyVasRecursive(AvlTreeGetRight(node), new_vas);

    struct vas_entry* entry = AvlTreeGetData(node);

    if (entry->lock) {
        /*
        * Got to add the new entry right now. We know it must be in memory as it
        * is locked.
        */
        assert(entry->in_ram);

        if (entry->allocated) {
            /*
            * Copy the physical page. We do this by copying the data into a buffer,
            * putting a new physical page in the existing VAS and then copying the 
            * data there. Then the original physical page that was there is free to use
            * as the copy.
            */
            uint8_t page_data[ARCH_PAGE_SIZE];
            inline_memcpy(page_data, (void*) entry->virtual, ARCH_PAGE_SIZE);
            size_t new_physical = entry->physical;
            entry->physical = AllocPhys();
            ArchUpdateMapping(GetVas(), entry);
            ArchFlushTlb(GetVas());
            inline_memcpy((void*) entry->virtual, page_data, ARCH_PAGE_SIZE);

            struct vas_entry* new_entry = AllocHeap(sizeof(struct vas_entry));
            *new_entry = *entry;
            new_entry->ref_count = 1;
            new_entry->physical = new_physical;
            new_entry->allocated = true;
            AvlTreeInsert(new_vas->mappings, entry);        // don't need to insert global - we're copying so it's already in global
            ArchAddMapping(new_vas, entry);

        } else {
            LogWriteSerial("fork() on a hardware-mapped page is not implemented yet");
            PanicEx(PANIC_NOT_IMPLEMENTED, "CopyVasRecursive");
        }
        
    } else {
        /*
        * If it's on swap, it's okay to still mark it as COW, as when we reload we will
        * try to do the 'copy'-on-write, and then we will reload from swap, and it will
        * then reload and then be copied. Alternatively, if it is read, then it gets brought
        * back into memory, but as a COW page still.
        *
        * BSS memory works fine like this too (but will incur another fault when it is used).
        *
        * At this stage (where shared memory doesn't exist yet), file mapped pages will also
        * be COWed. This means there will two copies of the file in memory should they write
        * to it. The final process to release memory will ultimately 'win' and have its changes
        * perserved to disk (the others will get overwritten).
        */
        entry->cow = true;
        entry->ref_count++;

        // again, no need to add to global - it's already there!
        AvlTreeInsert(new_vas->mappings, entry);

        ArchUpdateMapping(GetVas(), entry);
        ArchAddMapping(new_vas, entry);
    }
}

struct vas* CopyVas(void) {
    struct vas* vas = GetVas();
    struct vas* new_vas = CreateVas();

    AcquireSpinlockIrql(&vas->lock);
    // no need to change global - it's already there!
    CopyVasRecursive(AvlTreeGetRootNode(vas->mappings), new_vas);
    ArchFlushTlb(vas);
    ReleaseSpinlockIrql(&vas->lock);

    return new_vas;
}

struct vas* GetVas(void) {
    // TODO: cpu probably needs to have a lock object in it called current_vas_lock, which needs to be held whenever
    //       someone reads or writes to  current_vas;
    return GetCpu()->current_vas;
}

void SetVas(struct vas* vas) {
    GetCpu()->current_vas = vas;
    ArchSetVas(vas);
}

struct vas* GetKernelVas(void) {
    return kernel_vas;
}

void InitVirt(void) {
    // TODO: cpu probably needs to have a lock object in it called current_vas_lock, which needs to be held whenever
    //       someone reads or writes to current_vas;

    assert(!virt_initialised);
    GetCpu()->global_vas_mappings = AvlTreeCreate();
    AvlTreeSetComparator(GetCpu()->global_vas_mappings, VirtAvlComparator);
    ArchInitVirt();

    kernel_vas = GetVas();
    virt_initialised = true;
}


/**
 * Handles a page fault. Only to be called by the low-level, platform specific interrupt handler when a page
 * fault occurs. It will attempt to resolve any fault (e.g. handling copy-on-write, swapfile, file-backed, etc.).
 * 
 * @param faulting_virt The virtual address that was accessed that caused the page fault
 * @param fault_type The reason why a page fault occured. Is a bitfield of VM_WRITE, VM_READ, VM_USER and VM_EXEC.
 *                   VM_READ should be set if a non-present page was accessed. VM_USER should be set for permission
 *                   faults, and VM_WRITE should be set if the operation was caused by a write (as opposed to a read).
 *                   VM_EXEC should be set if execution tried to occur in a non-executable page.
 * 
 * @maxirql IRQL_PAGE_FAULT 
 */
void HandleVirtFault(size_t faulting_virt, int fault_type) {
    LogWriteSerial("HandleVirtFault A, 0x%X, %d\n", faulting_virt, fault_type);
    int irql = RaiseIrql(IRQL_PAGE_FAULT);

    (void) fault_type;

    struct vas* vas = GetVas();
    AcquireSpinlockIrql(&vas->lock);
    struct vas_entry* entry = GetVirtEntry(vas, faulting_virt);

    if (entry == NULL) {
        Panic(PANIC_PAGE_FAULT_IN_NON_PAGED_AREA);
    }
    LogWriteSerial("\nentry: v 0x%X, p 0x%X. aclrf = %d%d%d%d%d\n", entry->virtual, entry->physical, entry->allocated, entry->cow, entry->lock, entry->in_ram, entry->file);
    LogWriteSerial("HandleVirtFault B\n");

    /*
     * Sanity check that our flags are configured correctly.
     */
    assert(!(entry->in_ram && entry->swapfile));
    assert(!(entry->file && entry->swapfile));
    assert(!(!entry->in_ram && entry->lock));
    assert(!(entry->cow && entry->lock));

    // TODO: check for access violations (e.g. user using a supervisor page)
    //          (read / write is not necessarily a problem, e.g. COW)

    int result = BringIntoMemory(vas, entry, fault_type & VM_WRITE);
    if (result != 0) {
        Panic(PANIC_UNKNOWN);
    }

    ReleaseSpinlockIrql(&vas->lock);
    LowerIrql(irql);
    LogWriteSerial("Page fault totally done.\n");
}

/**
 * Determines whether or not virtual memory has been initialised yet. This can be used to determine if it
 * is possible to call any virtual memory functions (e.g. in the physical memory and heap allocators).
 * 
 * @return True if virtual memory is available, false otherwise.
 * 
 * @maxirql IRQL_HIGH
 */
bool IsVirtInitialised(void) {
    return virt_initialised;
}

size_t BytesToPages(size_t bytes) {
    return (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
}

void DestroyVas(struct vas* vas) {
    /*
     * TODO: implement this
     */
    (void) vas;

    // TODO: may need to add reference counting later on (depending on what we need),
    //       just decrement here, and only delete if got to 0.
}