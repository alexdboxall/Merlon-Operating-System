
#include <virtual.h>
#include <tree.h>
#include <debug.h>
#include <heap.h>
#include <common.h>
#include <arch.h>
#include <physical.h>
#include <dirent.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <timer.h>
#include <driver.h>
#include <thread.h>
#include <spinlock.h>
#include <log.h>
#include <sys/types.h>
#include <irql.h>
#include <irq.h>
#include <cpu.h>
#include <transfer.h>
#include <console.h>
#include <swapfile.h>
#include <vfs.h>
#include <errno.h>

// TODO: lots of locks! especially the global cpu one

static struct vas_entry* GetVirtEntry(struct vas* vas, size_t virtual);
static size_t SplitLargePageEntryIntoMultiple(
    struct vas* vas, size_t virtual, struct vas_entry* entry, int num_to_leave
);


/**
 * Stores a pointer to any kernel VAS. Ensures that when processes are 
 * destroyed, we are using a VAS that is different from the VAS that's being 
 * deleted. 
 */
static struct vas* kernel_vas;

static bool virt_initialised = false;

static int VirtAvlComparator(void* a, void* b) {
    struct vas_entry* a_entry = (struct vas_entry*) a;
    struct vas_entry* b_entry = (struct vas_entry*) b;

    assert((a_entry->virtual & (ARCH_PAGE_SIZE - 1)) == 0);
    assert((b_entry->virtual & (ARCH_PAGE_SIZE - 1)) == 0);

    /*
     * Check for overlapping regions for multi-mapping entries, and count that 
     * as equal. This allows us to return the correct entry if one of them is 
     * part of a multi-mapping entry.
     */
    size_t a_page = a_entry->virtual / ARCH_PAGE_SIZE;
    size_t b_page = b_entry->virtual / ARCH_PAGE_SIZE;
    if (a_page >= b_page && a_page < b_page + b_entry->num_pages) {
        return 0;
    }
    if (b_page >= a_page && b_page < a_page + a_entry->num_pages) {
        return 0;
    }

    return COMPARE_SIGN(a_entry->virtual, b_entry->virtual);
}

/**
 * Initialises a virtual address space in an already allocated section of memory.
 * 
 * @param vas The memory to initialise a virtual address space object in.
 * @param flags Can be 0 or VAS_NO_ARCH_INIT. If VAS_NO_ARCH_INIT is provided, 
 *              then no architecture-specific code will be called. This flag 
 *              should only be set if called by architecture-specific functions.
 */
void CreateVasEx(struct vas* vas, int flags) {
    MAX_IRQL(IRQL_SCHEDULER);

    vas->mappings = TreeCreate();
    InitSpinlock(&vas->lock, "vas", IRQL_SCHEDULER);
    TreeSetComparator(vas->mappings, VirtAvlComparator);
    if (!(flags & VAS_NO_ARCH_INIT)) {
        ArchInitVas(vas);
    }
}

struct vas* CreateVas() {
    MAX_IRQL(IRQL_SCHEDULER);

    struct vas* vas = AllocHeap(sizeof(struct vas));
    CreateVasEx(vas, 0);
    return vas;
}

struct defer_disk_access {
    struct file* file;
    struct vas_entry* entry;
    off_t offset;
    size_t address;
    int direction;
    bool deallocate_swap_on_read;
};

static void PerformDeferredAccess(void* data) {
    // TODO: see comment in BringIntoMemoryFromFile

    struct defer_disk_access* access = (struct defer_disk_access*) data;

    bool write = access->direction == TRANSFER_WRITE;
    size_t target_address = access->address;
    if (!write) {
        LogWriteSerial("RELOADING A PAGE!\n");
        
        /*
         * If we're reading, the page is not yet allocated or in memory (this is
         * so we don't have other threads trying to use the partially-filled 
         * page). Therefore, we allocate a temporary page to write the data in,
         * and we can allocate the page and copy the data while we hold a lock.
         * 
         * We can't just allocate the proper page entry now, as we can't hold 
         * the spinlock over the call to ReadFile.
         */
        target_address = MapVirt(0, 0, ARCH_PAGE_SIZE, VM_LOCK | VM_READ | VM_WRITE, NULL, 0);
    }

    struct transfer tr = CreateKernelTransfer(
        (void*) target_address, ARCH_PAGE_SIZE, access->offset, access->direction
    );

    int res = (write ? WriteFile : ReadFile)(access->file, &tr);
    if (res != 0) {
        /*
         * TODO: it's not actually always a failure. the only 'panic' condition 
         *       is when it involves the swapfile, but this code is also used 
         *       for dealing with normal file-mapped pages.
         * 
         *       for file-mapped pages, failures due to reading past the end of 
         *       the file should always be okay - we need to fill the rest of 
         *       the page with zero though (even if that page has no file data 
         *       on it, e.g. if we read really past the end of the array).
         */
        if (access->entry->swapfile) {
            Panic(PANIC_DISK_FAILURE_ON_SWAPFILE);
        } else {
            // I think for reads, it's okay to not do anything here on error, 
            // and just make use of the number of bytes that were actually 
            // transfered (and therefore complete failure means we just end up
            // with a blanked-out page being allocated).
            Panic(PANIC_NOT_IMPLEMENTED);
        }
    }

    if (write) {
        UnmapVirt(access->address, ARCH_PAGE_SIZE);

    } else {
        LogWriteSerial("RELOADING A PAGE! (B)\n");

        /*
         * Now we can actually lock the page and allocate the actual mapping.
         */
        struct vas* vas = GetVas();
        AcquireSpinlock(&vas->lock);

        struct vas_entry* entry = GetVirtEntry(vas, access->address);
        assert(entry->num_pages == 1);
        assert(entry->swapfile || entry->file);

        entry->lock = true;
        entry->physical = AllocPhys();
        entry->allocated = true;
        entry->allow_temp_write = true;
        entry->in_ram = true;
        entry->swapfile = false;
        ArchUpdateMapping(vas, entry);
        ArchFlushTlb(vas);

        // TODO: this should use the actual amount that was read...

        inline_memcpy((void*) access->address, (const char*) target_address, ARCH_PAGE_SIZE);
        
        entry->allow_temp_write = false;

        /*
         * If it was on the swapfile, we now need to mark that slot in the 
         * swapfile as free for future use.
         */
        if (access->deallocate_swap_on_read) {
            DeallocSwap(access->offset / ARCH_PAGE_SIZE);
        }

        /*
         * Don't perform relocations on the first load, as the first load will 
         * be when 'proper' relocation happens (i.e. the 'all at once' 
         * relocations) - and therefore the quick relocation table will not be 
         * created yet and we'll crash.
         * 
         * The reason we can't just not do the initial big relocation and make 
         * it all work though demand loading is because not all pages with 
         * driver code/data end up being marked as VM_RELOCATABLE (e.g. for 
         * small parts of data segments, etc.).
         */
        bool needs_relocations = entry->relocatable && !entry->first_load;
        LogWriteSerial("needs_relocations = %d\n", needs_relocations);

        ArchUpdateMapping(vas, entry);
        ArchFlushTlb(vas);

        /*
         * Need to keep page locked if we're doing relocations on it - otherwise
         * by the time that we actually load in all the data we need to do the 
         * relocations (e.g. ELF headers, the symbol table), we have probably
         * already swapped out the page we are relocating (which leads to us 
         * getting nowhere).
         */
        if (!needs_relocations) {
            entry->first_load = false;
            entry->load_in_progress = false;
            entry->lock = false;
        }
        ReleaseSpinlock(&vas->lock);

        UnmapVirt(target_address, ARCH_PAGE_SIZE);

        if (needs_relocations) {
            RelocatePage(vas, entry->relocation_base, access->address);
            AcquireSpinlock(&vas->lock);
            entry->first_load = false;
            entry->load_in_progress = false;
            UnlockVirtEx(vas, access->address);
            ReleaseSpinlock(&vas->lock);
        }
    }

    FreeHeap(access);
}

/**
 * Given a virtual page, it defers a write to disk. It creates a copy of the 
 * virtual page, so that it may be safely deleted as soon as this gets called.
 */
static void DeferDiskWrite(size_t old_addr, struct file* file, off_t offset) {
    size_t new_addr = MapVirt(
        0, 0, ARCH_PAGE_SIZE, 
        VM_LOCK | VM_READ | VM_WRITE | VM_RECURSIVE, NULL, 0
    );
    
    inline_memcpy((void*) new_addr, (const char*) old_addr, ARCH_PAGE_SIZE);
    
    struct defer_disk_access* access = AllocHeap(sizeof(struct defer_disk_access));
    access->address = new_addr;
    access->file = file;
    access->direction = TRANSFER_WRITE;
    access->offset = offset;
    access->deallocate_swap_on_read = false;
    DeferUntilIrql(IRQL_STANDARD_HIGH_PRIORITY, PerformDeferredAccess, (void*) access);
}

static void DeferDiskRead(
    size_t new_addr, struct file* file, off_t offset, bool deallocate_swap_on_read
) {
    struct defer_disk_access* access = AllocHeap(sizeof(struct defer_disk_access));
    access->address = new_addr;
    access->file = file;
    access->direction = TRANSFER_READ;
    access->offset = offset;
    access->deallocate_swap_on_read = deallocate_swap_on_read;
    DeferUntilIrql(IRQL_STANDARD_HIGH_PRIORITY, PerformDeferredAccess, (void*) access);
}

/**
 * Evicts a particular page mapping from virtual memory, freeing up its physical
 * page (if it had one). This will often involve accessing the disk to put it on
 * swapfile (or save modifications to a file-backed page).
 */
void EvictPage(struct vas* vas, struct vas_entry* entry) {
    EXACT_IRQL(IRQL_STANDARD);   

    LogWriteSerial("-------> EVICTING 0x%X\n", entry->virtual);

    assert(!entry->lock);
    assert(!entry->cow);
    assert(entry->in_ram);

    AcquireSpinlock(&vas->lock);

    if (entry->file) {
        if (entry->write && !entry->relocatable) {
            DeferDiskWrite(entry->virtual, entry->file_node, entry->file_offset);
        }

        entry->in_ram = false;
        entry->allocated = false;
        DeallocPhys(entry->physical);
        ArchUnmap(vas, entry);
        ArchFlushTlb(vas);

    } else {
        entry->in_ram = false;
        entry->swapfile = true;
        entry->allocated = false;
        
        uint64_t offset = AllocSwap() * ARCH_PAGE_SIZE;
        DeferDiskWrite(entry->virtual, GetSwapfile(), offset);
        entry->swapfile_offset = offset;

        ArchUnmap(vas, entry);
        DeallocPhys(entry->physical);
        ArchFlushTlb(vas);
    }

    LogWriteSerial("Evicted page... A\n");
    ReleaseSpinlock(&vas->lock);
    LogWriteSerial("Evicted page... B\n");
}

/*
 * Lower value means it should be swapped out first.
 */
static int GetPageEvictionRank(struct vas* vas, struct vas_entry* entry) {
    bool accessed;
    bool dirty;
    ArchGetPageUsageBits(vas, entry, &accessed, &dirty);

    int penalty = (entry->global ? 3 : 0) + entry->times_swapped * 8;

    if (entry->evict_first) {
        return entry->times_swapped;

    } else if (entry->relocatable) {
        return 150;

    } else if (entry->cow) {
        return 90 + penalty;

    } else if (entry->file && !entry->write) {
        return (accessed ? 30 : 10) + penalty;

    } else if (entry->file && entry->write) {
        return (dirty ? 70 : (accessed ? 40 : 20)) + penalty;
    
    } else if (!dirty) {
        return (accessed ? 60 : 50) + penalty;

    } else {
        return 80 + penalty;
    }
}

struct eviction_candidate {
    struct vas* vas;
    struct vas_entry* entry;
};

#define PREV_SWAP_LIMIT 64

void FindVirtToEvictRecursive(
    struct vas* vas, 
    struct tree_node* node, 
    int* lowest_rank, 
    struct eviction_candidate* lowest_ranked, 
    int* count, 
    struct vas_entry** prev_swaps
) {
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
     * After scanning through 500 entries, we'll allow early exits for less 
     * optimal pages.
     */
    int limit = (((*count - 500) / 75) + 10);
    if (*count > 500 && *lowest_rank < limit) {
        return;
    }

    // TODO: we really need a better page swapper that doesn't just pick the
    //       first page it sees.
    
    struct vas_entry* entry = node->data;
    if (!entry->lock && entry->allocated && entry->in_ram) {
        int rank = GetPageEvictionRank(vas, entry);

        /*
         * To ensure we mix up who gets evicted, when there's an equality, we 
         * use it 1/4 times. It is likely there are more than 4 to replace, so 
         * this ensures that we cycle through many of them.
         */
        bool equal = rank == *lowest_rank;
        if (equal) {
            equal = (rand++ & 3) == 0;
        }

        bool prev_swap = false;
        for (int i = 0; i < PREV_SWAP_LIMIT; ++i) {
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

    FindVirtToEvictRecursive(vas, node->left, lowest_rank, lowest_ranked, count, prev_swaps);
    FindVirtToEvictRecursive(vas, node->right, lowest_rank, lowest_ranked, count, prev_swaps);
}

void FindVirtToEvict(
    struct vas* vas, 
    int* lowest_rank, 
    struct eviction_candidate* lowest_ranked, 
    bool include_globals, 
    struct vas_entry** prev_swaps
) {
    int count = 0;
    FindVirtToEvictRecursive(vas, vas->mappings->root, lowest_rank, lowest_ranked, &count, prev_swaps);
    if (include_globals) {
        FindVirtToEvictRecursive(vas, GetCpu()->global_vas_mappings->root, lowest_rank, lowest_ranked, &count, prev_swaps);
    }
}

/**
 * Searches through virtual memory (that doesn't necessarily have to be in the 
 * current virtual address space), and finds and evicts a page of virtual 
 * memory, to try free up physical memory. 
 */
void EvictVirt(void) {
    MAX_IRQL(IRQL_PAGE_FAULT);   
    
    if (GetSwapfile() == NULL) {
        return;
    }

    // TODO: we need to ensure that EvictVirt(), when called from the defer, does not evict any pages that were just
    //       loaded in!! This is an issue when we need to perform relocations during page faults, as that brings in a 
    //       whole heap of other pages, and that often causes TryEvictPages() to straight away get rid of the page we just
    //       loaded in. Alternatively, TryEvictPages() can be a NOP the first time it is called after a page fault.
    //       This would give the code on the page that we loaded in time to 'progress' before being swapped out again.

    // don't allow any of the last 8 swaps to be repeated (as an instruction may require at least 6 pages on x86
    // if it straddles many boundaries)
    static struct vas_entry* previous_swaps[PREV_SWAP_LIMIT] = {0};
    static int swap_num = 0;

    int lowest_rank = 10000;
    struct eviction_candidate lowest_ranked;
    lowest_ranked.entry = NULL;
    
    AcquireSpinlock(&GetVas()->lock);
    FindVirtToEvict(GetVas(), &lowest_rank, &lowest_ranked, true, previous_swaps);
    ReleaseSpinlock(&GetVas()->lock);

    // TODO: go through other address spaces

    if (lowest_ranked.entry != NULL) {
        previous_swaps[swap_num++ % PREV_SWAP_LIMIT] = lowest_ranked.entry;     
        EvictPage(lowest_ranked.vas, lowest_ranked.entry);
        lowest_ranked.entry->times_swapped++;
    }
}

static void InsertIntoAvl(struct vas* vas, struct vas_entry* entry) {
    assert(IsSpinlockHeld(&vas->lock));
    
    if (entry->global) { 
        AcquireSpinlock(&GetCpu()->global_mappings_lock);
        TreeInsert(GetCpu()->global_vas_mappings, entry);
        ReleaseSpinlock(&GetCpu()->global_mappings_lock);

    } else {        
        TreeInsert(vas->mappings, entry);
    }
}

static void DeleteFromAvl(struct vas* vas, struct vas_entry* entry) {
    // TODO: this probably needs to check a flag/counter to mark if someone used
    // GetVirtEntry and still has it acquired..., and if so, postpone the release

    assert(IsSpinlockHeld(&vas->lock));
    if (entry->global) {
        AcquireSpinlock(&GetCpu()->global_mappings_lock);
        TreeDelete(GetCpu()->global_vas_mappings, entry);
        ReleaseSpinlock(&GetCpu()->global_mappings_lock);

    } else {
        TreeDelete(vas->mappings, entry); 
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
static void AddMapping(struct vas* vas, size_t physical, size_t virtual, int flags, struct file* file, off_t pos, size_t number) {
    MAX_IRQL(IRQL_SCHEDULER);

    assert(!(file != NULL && (flags & VM_FILE) == 0));

    struct vas_entry* entry = AllocHeapZero(sizeof(struct vas_entry));
    entry->allocated = false;

    bool lock = flags & VM_LOCK;
    entry->lock = lock;
    entry->in_ram = lock;

    size_t relocation_base = 0;
    if (flags & VM_RELOCATABLE) {
        relocation_base = physical;
        physical = 0;
    }

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
    entry->read = (flags & VM_READ) ? 1 : 0;
    entry->write = (flags & VM_WRITE) ? 1 : 0;
    entry->exec = (flags & VM_EXEC) ? 1 : 0;
    entry->file = (flags & VM_FILE) ? 1 : 0;
    entry->user = (flags & VM_USER) ? 1 : 0;
    entry->share_on_fork = (flags & VM_SHARED) ? 1 : 0;
    entry->evict_first = (flags & VM_EVICT_FIRST) ? 1 : 0;
    entry->relocatable = (flags & VM_RELOCATABLE) ? 1 : 0;
    entry->allow_temp_write = false;
    entry->load_in_progress = false;
    entry->global = !(flags & VM_LOCAL);
    entry->physical = physical;
    entry->ref_count = 1;
    entry->file_offset = pos;
    entry->file_node = file;
    entry->swapfile = false;
    entry->first_load = entry->relocatable;
    entry->num_pages = number;

    if (entry->relocatable) {
        entry->relocation_base = relocation_base;
    } else {
        entry->swapfile_offset = 0xDEADDEAD;
    }

    LogWriteSerial("Adding mapping at 0x%X to vas 0x%X - num is %d. flags = 0x%X, rwxgu'lfia = %d%d%d%d%d'%d%d%d%d. p 0x%X.\n", 
        entry->virtual, vas, number, flags,
        entry->read, entry->write, entry->exec, entry->global, entry->user, entry->lock, entry->file, entry->in_ram, entry->allocated,
        entry->physical 
    );

    /*
     * TODO: later on, check if shared, and add phys->virt entry if needed
     */
    
    if ((flags & VM_RECURSIVE) == 0) {
        AcquireSpinlock(&vas->lock);
    }
    InsertIntoAvl(vas, entry);
    ArchAddMapping(vas, entry);

    if (entry->lock && (flags & VM_MAP_HARDWARE) == 0) {
        if (GetVas() == vas) {
            /*
             * Need to zero out the page - this must happen on first load in, and as we have to load in
             * locked pages now, we must do it now.
             */
            SetVirtPermissionsEx(vas, entry->virtual, VM_WRITE, 0);
            memset((void*) entry->virtual, 0, entry->num_pages * ARCH_PAGE_SIZE);
            if (!entry->write) {
                SetVirtPermissionsEx(vas, entry->virtual, 0, VM_WRITE);
            }
        } else {
            LogDeveloperWarning("yuck. PAGE HAS NOT BEEN ZEROED!\n");
        }
    }

    if ((flags & VM_RECURSIVE) == 0) {
        ReleaseSpinlock(&vas->lock);
    }
}

static bool IsRangeInUse(struct vas* vas, size_t virtual, size_t pages) {
    bool in_use = false;

    struct vas_entry dummy = {.num_pages = 1, .virtual = virtual};

    /*
     * We have to loop over the local one, and if it isn't there, the global one. We do this
     * in separate loops to prevent the need to acquire both spinlocks at once, which could lead
     * to a deadlock.
     */
    AcquireSpinlock(&vas->lock);
    for (size_t i = 0; i < pages; ++i) {
        if (TreeContains(vas->mappings, (void*) &dummy)) {
            in_use = true;
            break;
        }
        dummy.virtual += ARCH_PAGE_SIZE;
    }
    ReleaseSpinlock(&vas->lock);

    if (in_use) {
        return true;
    }

    AcquireSpinlock(&GetCpu()->global_mappings_lock);
    dummy.virtual = virtual;
    for (size_t i = 0; i < pages; ++i) {
        if (TreeContains(GetCpu()->global_vas_mappings, (void*) &dummy)) {
            in_use = true;
            break;
        }
        dummy.virtual += ARCH_PAGE_SIZE;
    }
    ReleaseSpinlock(&GetCpu()->global_mappings_lock);

    return in_use;
}

static size_t AllocVirtRange(struct vas* vas, size_t pages, int flags) {
    // TODO: !!!! @@@@ WE NEED LOCKS, NOW! EVEN FOR THE HIDEOUS VERSION
    //                 IT IS RACEY (I.E. MAY RETURN THE SAME RANGE
    //                 TWICE, CAUSING ALL SORTS OF A MESS AS BOTH USERS OF IT
    //                 TRY TO GIVE IT DIFFERENT MAPPINGS

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
        LogWriteSerial("hideously (A) 0x%X\n", retv);
        return retv;

    } else {
        /*
         * TODO: this probably needs a global lock of some sort.
         */
        static size_t hideous_allocator = ARCH_KRNL_SBRK_BASE;
        size_t retv = hideous_allocator;
        hideous_allocator += pages * ARCH_PAGE_SIZE;
        LogWriteSerial("hideous 0x%X\n", retv);
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
 * All mapped pages will be zeroed out (either on first use, or if locked, when allocated) - except if VM_MAP_HARDWARE or
 * VM_FILE is set. If VM_FILE is set, reading beyond the end of the file, but within the page limit, will read zeroes.
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
 *                      VM_WRITE        : if set, the page will be marked as writable. On some architectures, this may have the
 *                                        effect of implying VM_READ as well.
 *                      VM_USER         : if set, then usermode can access this page without faulting
 *                      VM_EXEC         : if set, then code can be executed in this page
 *                      VM_LOCK         : if set, the page will immediately get a physical memory backing, and will not be
 *                                        paged out
 *                      VM_FILE         : if set, this page is file-backed. Cannot be combined with VM_MAP_HARDWARE. 
 *                                        Cannot be combined with VM_LOCK.
 *                      VM_MAP_HARDWARE : if set, a physical address can be specified for the backing. If this flag is set,
 *                                        then VM_LOCK must also be set, and VM_FILE must be clear.
 *                      VM_LOCAL        : if set, it is only mapped into the current virtual address space. If set, it is mapped
 *                                        into the kernel virtual address space.
 *                      VM_RECURSIVE    : must be set if and only if this call to MapVirtEx is being called with the virtual
 *                                        address space lock already held. Does not affect the page, only the call to MapVirtEx
 *                                        When set, the lock is not automatically acquired or released as it is assumed to be
 *                                        already held.
 *                      VM_RELOCATABLE  : if set, then this page will have driver relocations applied to it when it is swapped
 *                                        in. VM_FILE must be set as well. `file` should be set to the driver's file.
 *                      VM_FIXED_VIRT   : if set, then the virtual address specified in `virtual` will be required to be used -
 *                                        if any page required for the mapping of this size is already allocated, the allocation
 *                                        will fail. If clear, then another virtual address may be used in order to satisfy a
 *                                        request.
 *                      VM_EVICT_FIRST  : indicates to the virtual memory manager that when memory is low, this page should be
 *                                        evicted before other pages
 * @param file      If VM_FILE is set, then the page is backed by this file, starting at the position specified by pos.
 *                      If VM_FILE is clear, then this value must be NULL.
 * @param pos       If VM_FILE is set, then this is the offset into the file where the page is mapped to. If VM_FILE is clear,
 *                      then this value must be 0.
 * 
 * @maxirql IRQL_SCHEDULER
 */
#define RETURN_FAIL_IF(err, cond) if (cond) {*error = err; return 0;}
size_t MapVirtEx(struct vas* vas, size_t physical, size_t virtual, size_t pages, int flags, struct file* file, off_t pos, int* error) {
    MAX_IRQL(IRQL_SCHEDULER);

    *error = 0;

    RETURN_FAIL_IF(EINVAL, physical != 0 && (flags & (VM_MAP_HARDWARE | VM_RELOCATABLE)) == 0);
    RETURN_FAIL_IF(EINVAL, (flags & VM_MAP_HARDWARE) && (flags & VM_LOCK) == 0);
    RETURN_FAIL_IF(EINVAL, (flags & VM_FILE) && (flags & VM_MAP_HARDWARE));
    RETURN_FAIL_IF(EINVAL, (flags & VM_FILE) && file == NULL);
    RETURN_FAIL_IF(EINVAL, (flags & VM_FILE) && (flags & VM_LOCK));
    RETURN_FAIL_IF(EINVAL, (flags & VM_FILE) == 0 && (file != NULL || pos != 0));
    RETURN_FAIL_IF(EINVAL, (flags & VM_RELOCATABLE) && (flags & VM_FILE) == 0);
    RETURN_FAIL_IF(EINVAL, (flags & VM_RELOCATABLE) && (flags & VM_USER));
    RETURN_FAIL_IF(EINVAL, (flags & VM_RELOCATABLE) && physical == 0);
    RETURN_FAIL_IF(EINVAL, (flags & VM_LOCK) && (flags & VM_SHARED));
    RETURN_FAIL_IF(EACCES, (flags & VM_FILE) && !(IFTODT(file->node->stat.st_mode) == DT_REG || IFTODT(file->node->stat.st_mode) == DT_BLK));
    RETURN_FAIL_IF(EACCES, (flags & VM_FILE) && !file->can_read);
    RETURN_FAIL_IF(EACCES, (flags & VM_FILE) && !file->can_write && (flags & VM_WRITE));

    /*
     * Get a virtual memory range that is not currently in use.
     */
    if (virtual == 0) {
        virtual = AllocVirtRange(vas, pages, flags & VM_LOCAL);

    } else {
        // TODO: need to lock here to make the israngeinuse and allocvirtrange to be atomic
        if (IsRangeInUse(vas, virtual, pages)) {
            if (flags & VM_FIXED_VIRT) {
                *error = EEXIST;
                return 0;
            }

            virtual = AllocVirtRange(vas, pages, flags & VM_LOCAL);
        }
    }

    /*
     * No point doing the multi-page mapping with only 2 pages, as the splitting cost is probably
     * going to be greater than actually just adding 2 pages in the first place.
     * 
     * May want to increase this value furher in the future (e.g. maybe to 4 or 8)?
     */
    bool multi_page_mapping = (((flags & VM_LOCK) == 0) || ((flags & VM_MAP_HARDWARE) != 0)) && pages >= 3;

    for (size_t i = 0; i < (multi_page_mapping ? 1 : pages); ++i) {
        if (flags & VM_FILE) {
            ReferenceFile(file);
        }
        AddMapping(
            vas, 
            (flags & VM_RELOCATABLE) ? physical : (physical == 0 ? 0 : (physical + i * ARCH_PAGE_SIZE)), 
            virtual + i * ARCH_PAGE_SIZE, 
            flags, 
            file,
            pos + i * ARCH_PAGE_SIZE,
            multi_page_mapping ? pages : 1
        );
    }   

    if (vas == GetVas()) {
        ArchFlushTlb(vas);
    }
    
    return virtual;
}

/**
 * Creates a virtual memory mapping in the current virtual address space.
 */
size_t MapVirt(size_t physical, size_t virtual, size_t bytes, int flags, struct file* file, off_t pos) {
    int error;
    (void) error;
    return MapVirtEx(GetVas(), physical, virtual, BytesToPages(bytes), flags, file, pos, &error);
}

static struct vas_entry* GetVirtEntry(struct vas* vas, size_t virtual) {
    assert(IsSpinlockHeld(&vas->lock));

    struct vas_entry dummy = {.num_pages = 1, .virtual = virtual & ~(ARCH_PAGE_SIZE - 1)};
    struct vas_entry* res = (struct vas_entry*) TreeGet(vas->mappings, (void*) &dummy);
    if (res == NULL) {
        AcquireSpinlock(&GetCpu()->global_mappings_lock);
       
        // TODO: possible mark the page as in use - but will need to add a lot of release calls around the place
            /*
            * Actually, I think the better idea is to make page locks use a counter instead
            * of a flag, an have this increment that counter while the global lock is held
            * Then any callers of GetVirtEntry must remember to UnlockVirt(Ex) afterwards
            */

        res = (struct vas_entry*) TreeGet(GetCpu()->global_vas_mappings, (void*) &dummy);
        ReleaseSpinlock(&GetCpu()->global_mappings_lock);
    }
    return res;
}

size_t GetPhysFromVirt(size_t virtual) {
    struct vas* vas = GetVas();
    AcquireSpinlock(&vas->lock);
    struct vas_entry* entry = GetVirtEntry(vas, virtual);
    size_t result = entry->physical;

    /*
     * Handle mappings of more than 1 page at a time by adding the extra offset
     * from the start of the mapping. 
     */
    size_t target_page = virtual / ARCH_PAGE_SIZE;
    size_t entry_page = entry->virtual / ARCH_PAGE_SIZE;
    if (entry_page < target_page) {
        result += (target_page - entry_page) * ARCH_PAGE_SIZE;
    }

    ReleaseSpinlock(&vas->lock);
    return result;
}

static size_t SplitLargePageEntryIntoMultiple(struct vas* vas, size_t virtual, struct vas_entry* entry, int num_to_leave) {
    if (entry->num_pages == 1) {
        return 1;
    }

    if (entry->ref_count != 1) {
        LogDeveloperWarning("Splitting multi-mapping with ref_count != 1, this hasn't been tested!\n");
    }

    /*
     * Although it can't be allocated, it can be in RAM (e.g. for VM_MAP_HARDWARE).
     */
    assert(!entry->allocated);
    assert(!entry->swapfile);

    size_t entry_page = entry->virtual / ARCH_PAGE_SIZE;
    size_t target_page = virtual / ARCH_PAGE_SIZE;

    /*
     * Split off anything before this page.
     */
    if (entry_page < target_page) {
        LogWriteSerial("Made a split! (A)\n");
        size_t num_beforehand = target_page - entry_page;

        struct vas_entry* pre_entry = AllocHeap(sizeof(struct vas_entry));
        *pre_entry = *entry;

        pre_entry->num_pages = num_beforehand;
        entry->num_pages -= num_beforehand;
        entry->virtual += num_beforehand * ARCH_PAGE_SIZE;

        /*
         * For multi-mapping for VM_MAP_HARDWARE
         */
        if (entry->physical != 0) {
            entry->physical += num_beforehand * ARCH_PAGE_SIZE;
        }

        if (entry->file) {
            entry->file_offset += num_beforehand * ARCH_PAGE_SIZE;
        }

        InsertIntoAvl(vas, pre_entry);
    }

    /*
     * There's now no pages beforehand. Now we need to check if there are any other pages
     * after this.
     */
    if (entry->num_pages > num_to_leave) {   
        LogWriteSerial("Made a split! (B)\n");
     
        struct vas_entry* post_entry = AllocHeap(sizeof(struct vas_entry));
        *post_entry = *entry;

        post_entry->num_pages -= num_to_leave;
        entry->num_pages = num_to_leave;

        post_entry->virtual += ARCH_PAGE_SIZE * num_to_leave;

        /*
         * For multi-mapping for VM_MAP_HARDWARE
         */
        if (entry->physical != 0) {
            post_entry->physical += ARCH_PAGE_SIZE * num_to_leave;
        }

        if (entry->file) {
            post_entry->file_offset += ARCH_PAGE_SIZE * num_to_leave;
        }

        InsertIntoAvl(vas, post_entry);
    }

    return entry->num_pages;
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

static void BringIntoMemoryFromFile(struct vas_entry* entry, size_t faulting_virt) {
    // TODO: need to test that you're allowed to read past the end of the file (even into other pages)
    //       if the size mapped allows it, and just get zeros

    SplitLargePageEntryIntoMultiple(GetVas(), faulting_virt, entry, 1);
    entry->load_in_progress = true;
    ArchUpdateMapping(GetVas(), entry);
    ArchFlushTlb(GetVas());
    DeferDiskRead(entry->virtual, entry->file_node, entry->file_offset, false);
}

static void BringIntoMemoryFromSwapfile(struct vas_entry* entry) {
    assert(!entry->file);

    uint64_t offset = entry->swapfile_offset;
    entry->load_in_progress = true;
    ArchUpdateMapping(GetVas(), entry);
    ArchFlushTlb(GetVas());
    DeferDiskRead(entry->virtual, GetSwapfile(), offset, true);
}

static void BringInBlankPage(struct vas* vas, struct vas_entry* entry, size_t faulting_virt, int fault_type) {
    if ((fault_type & VM_READ) && !entry->read) {
        UnhandledFault();
    }
    if ((fault_type & VM_WRITE) && !entry->write) {
        UnhandledFault();
    }
    if ((fault_type & VM_EXEC) && !entry->exec) {
        UnhandledFault();
    }

    SplitLargePageEntryIntoMultiple(vas, faulting_virt, entry, 1);
    assert(entry->num_pages == 1);

    entry->physical = AllocPhys();
    entry->allocated = true;
    entry->in_ram = true;
    entry->allow_temp_write = true;
    assert(!entry->swapfile);
    ArchUpdateMapping(vas, entry);
    ArchFlushTlb(vas);

    inline_memset((void*) entry->virtual, 0, ARCH_PAGE_SIZE);
    entry->allow_temp_write = false;
    ArchUpdateMapping(vas, entry);
    ArchFlushTlb(vas);
}

static int BringIntoMemory(struct vas* vas, struct vas_entry* entry, bool allow_cow, size_t faulting_virt, int fault_type) {
    assert(IsSpinlockHeld(&vas->lock));

    if (entry->cow && allow_cow) {
        assert(entry->num_pages == 1);
        LogWriteSerial("--> COW\n");
        BringIntoMemoryFromCow(entry);
        return 0;
    }

    if (entry->file && !entry->in_ram) {
        LogWriteSerial("--> FILE\n");
        BringIntoMemoryFromFile(entry, faulting_virt);
        return 0;
    }

    if (entry->swapfile) {
        LogWriteSerial("--> SWAP\n");
        assert(entry->num_pages == 1);
        BringIntoMemoryFromSwapfile(entry);
        return 0;
    }

    if (!entry->in_ram) {
        LogWriteSerial("--> BSS\n");
        BringInBlankPage(vas, entry, faulting_virt, fault_type);
        return 0;
    }

    LogWriteSerial("--> UH-OH\n entry says: virt = 0x%X, file=%d, inram=%d, alloc=%d, prgs=%d", 
        entry->virtual, entry->file, entry->in_ram, entry->allocated, entry->load_in_progress
    );

    extern size_t* x86GetPageEntry(struct vas* vas, size_t virtual);
    LogWriteSerial("\nx86 has 0x%X\n", *x86GetPageEntry(vas, entry->virtual));
    
    return EINVAL;
}

bool LockVirtEx(struct vas* vas, size_t virtual) {
    struct vas_entry* entry = GetVirtEntry(vas, virtual);

    if (!entry->in_ram) {
        SplitLargePageEntryIntoMultiple(vas, virtual, entry, 1);
        int res = BringIntoMemory(vas, entry, true, virtual, 0);
        if (res != 0) {
            Panic(PANIC_CANNOT_LOCK_MEMORY);
        }
        assert(entry->in_ram);
    }

    bool old_lock = entry->lock;
    entry->lock = true;
    return old_lock;
}

void UnlockVirtEx(struct vas* vas, size_t virtual) {
    struct vas_entry* entry = GetVirtEntry(vas, virtual);
    SplitLargePageEntryIntoMultiple(vas, virtual, entry, 1);
    entry->lock = false;
}

bool LockVirt(size_t virtual) {
    struct vas* vas = GetVas();
    AcquireSpinlock(&vas->lock);
    bool res = LockVirtEx(vas, virtual);
    ReleaseSpinlock(&vas->lock);
    return res;
}

void UnlockVirt(size_t virtual) {
    struct vas* vas = GetVas();
    AcquireSpinlock(&vas->lock);
    UnlockVirtEx(vas, virtual);
    ReleaseSpinlock(&vas->lock);
}

int SetVirtPermissionsEx(struct vas* vas, size_t virtual, int set, int clear) {
    if ((set | clear) & ~(VM_READ | VM_WRITE | VM_EXEC | VM_USER)) {
        return EINVAL;
    }
    
    struct vas_entry* entry = GetVirtEntry(vas, virtual);
    if (entry == NULL) {
        return ENOMEM;
    }

    if (entry->file && !entry->file_node->can_write && (set & VM_WRITE) && !entry->relocatable) {
        return EACCES;
    }
        
    SplitLargePageEntryIntoMultiple(vas, virtual, entry, 1);

    entry->read = (set & VM_READ) ? true : (clear & VM_READ ? false : entry->read);
    entry->write = (set & VM_WRITE) ? true : (clear & VM_WRITE ? false : entry->write);
    entry->exec = (set & VM_EXEC) ? true : (clear & VM_EXEC ? false : entry->exec);
    entry->user = (set & VM_USER) ? true : (clear & VM_USER ? false : entry->user);

    ArchUpdateMapping(vas, entry);
    ArchFlushTlb(vas);
    return 0;
}

/*
 * Setting a bit overrides clearing it (i.e. it acts as though it clears first, 
 * and then sets).
 */
int SetVirtPermissions(size_t virtual, int set, int clear) {
    struct vas* vas = GetVas();
    AcquireSpinlock(&vas->lock);
    int retv = SetVirtPermissionsEx(vas, virtual, set, clear);
    ReleaseSpinlock(&vas->lock);
    return retv;
}

int GetVirtPermissions(size_t virtual) {
    struct vas* vas = GetVas();
    AcquireSpinlock(&vas->lock);
    struct vas_entry* entry_ptr = GetVirtEntry(GetVas(), virtual);
    if (entry_ptr == NULL) {
        ReleaseSpinlock(&vas->lock);
        return 0;
    }
    struct vas_entry entry = *entry_ptr;
    ReleaseSpinlock(&vas->lock);

    int permissions = 0;
    if (entry.read) permissions |= VM_READ;
    if (entry.write) permissions |= VM_WRITE;
    if (entry.exec) permissions |= VM_EXEC;
    if (entry.lock) permissions |= VM_LOCK;
    if (entry.file) permissions |= VM_FILE;
    if (entry.user) permissions |= VM_USER;
    if (!entry.global) permissions |= VM_LOCAL;
    if (entry.relocatable) permissions |= VM_RELOCATABLE;

    return permissions;
}

static bool DereferenceEntry(struct vas* vas, struct vas_entry* entry) {
    assert(entry->ref_count > 0);
    entry->ref_count--;
    
    size_t virtual = entry->virtual;
    bool needs_tlb_flush = false;

    if (entry->ref_count == 0) {
        if (entry->file && entry->write && entry->in_ram) { 
            DeferDiskWrite(entry->virtual, entry->file_node, entry->file_offset);
            // TODO: after that DeferDiskWrite, we need to defer a DereferenceFile(entry->file_node)
        }
        if (entry->in_ram) {
            ArchUnmap(vas, entry);
            needs_tlb_flush = true;
        }
        if (entry->swapfile) {
            assert(!entry->allocated);
            DeallocSwap(entry->physical / ARCH_PAGE_SIZE);
        }
        if (entry->allocated) {
            assert(!entry->swapfile);   // can't be on swap, as putting on swap clears allocated bit
            DeallocPhys(entry->physical);
        }

        ArchSetPageUsageBits(vas, entry, false, false);
        DeleteFromAvl(vas, entry);
        FreeHeap(entry);
        FreeVirtRange(vas, virtual, entry->num_pages);
    }

    return needs_tlb_flush;
}

static void WipeUsermodePagesRecursive(struct tree_node* node) {
    if (node == NULL) {
        return;
    }

    struct vas_entry* entry = node->data;
    if (entry->virtual >= ARCH_USER_STACK_LIMIT && entry->virtual < ARCH_PROG_LOADER_BASE) {
        LogWriteSerial("WIPING A USERMODE PAGE ON EXEC: 0x%X\n", entry->virtual);
        DereferenceEntry(GetVas(), entry);
    }

    if (entry->virtual >= ARCH_USER_STACK_LIMIT) {
        WipeUsermodePagesRecursive(node->left);
    }
    if (entry->virtual < ARCH_PROG_LOADER_BASE) {
        WipeUsermodePagesRecursive(node->right);
    }
}

int WipeUsermodePages(void) {
    struct vas* vas = GetVas();
    AcquireSpinlock(&vas->lock);
    WipeUsermodePagesRecursive(GetVas()->mappings->root);
    ArchFlushTlb(vas);
    ReleaseSpinlock(&vas->lock); 
    return 0;  
}

int UnmapVirtEx(struct vas* vas, size_t virtual, size_t pages, int flags) {
    bool needs_tlb_flush = false;

    for (size_t i = 0; i < pages; ++i) {
        struct vas_entry* entry = GetVirtEntry(vas, virtual + i * ARCH_PAGE_SIZE);
        if (entry == NULL) {
            if (flags & VMUN_ALLOW_NON_EXIST) {
                continue;
            } else {
                return EINVAL;
            }
        }

        SplitLargePageEntryIntoMultiple(vas, virtual, entry, 1);        // TODO: multi-pages
        needs_tlb_flush |= DereferenceEntry(vas, entry);
    }

    if (needs_tlb_flush) {
        ArchFlushTlb(vas);
    }

    return 0;
}

int UnmapVirt(size_t virtual, size_t bytes) {
    struct vas* vas = GetVas();
    AcquireSpinlock(&vas->lock);
    int res = UnmapVirtEx(vas, virtual, BytesToPages(bytes), 0);
    ReleaseSpinlock(&vas->lock);
    return res;
}

static void CopyVasRecursive(struct tree_node* node, struct vas* new_vas) {
    LogWriteSerial("[CopyVasRecursive]: node 0x%X\n", node);

    if (node == NULL) {
        return;
    }

    struct vas_entry* entry = node->data;
    LogWriteSerial("[CopyVasRecursive]: entry is at 0x%X\n", entry);
    LogWriteSerial("[CopyVasRecursive]: entry with virt addr 0x%X\n", entry->virtual);

    if (entry->lock) {
        /*
        * Got to add the new entry right now. We know it must be in memory as it
        * is locked.
        */
        LogWriteSerial("[CopyVasRecursive]: locked entry...\n");

        assert(entry->in_ram);
        assert(!entry->share_on_fork);

        if (entry->allocated) {
            /*
            * Copy the physical page. We do this by copying the data into a buffer,
            * putting a new physical page in the existing VAS and then copying the 
            * data there. Then the original physical page that was there is free to use
            * as the copy.
            */
            uint8_t page_data[ARCH_PAGE_SIZE];  // TODO: MapVirt this ? 
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
            TreeInsert(new_vas->mappings, entry);        // don't need to insert global - we're copying so it's already in global
            ArchAddMapping(new_vas, entry);

        } else {
            PanicEx(PANIC_UNKNOWN, "can't fork with a hardware-mapped page");
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
        LogWriteSerial("[CopyVasRecursive]: normal entry...\n");

        if (!entry->share_on_fork) {
            entry->cow = true;
        }
        entry->ref_count++;

        // again, no need to add to global - it's already there!
        TreeInsert(new_vas->mappings, entry);
        LogWriteSerial("[CopyVasRecursive]: inserted into tree...\n");

        ArchUpdateMapping(GetVas(), entry);
        LogWriteSerial("[CopyVasRecursive]: updated arch...\n");

        ArchAddMapping(new_vas, entry);
        LogWriteSerial("[CopyVasRecursive]: added to arch...\n");
    }

    LogWriteSerial("[CopyVasRecursive]: node->left 0x%X\n", node->left);
    LogWriteSerial("[CopyVasRecursive]: node->right 0x%X\n", node->right);

    CopyVasRecursive(node->left, new_vas);
    CopyVasRecursive(node->right, new_vas);
}

struct vas* CopyVas(void) {
    struct vas* vas = GetVas();
    struct vas* new_vas = CreateVas();
    LogWriteSerial("[CopyVas]: created new...\n");
    AcquireSpinlock(&vas->lock);
    LogWriteSerial("[CopyVas]: locked...\n");
    // no need to change global - it's already there!
    CopyVasRecursive(vas->mappings->root, new_vas);
    LogWriteSerial("[CopyVas]: recursion done...\n");
    ArchFlushTlb(vas);
    LogWriteSerial("[CopyVas]: tlb flushed...\n");
    ReleaseSpinlock(&vas->lock);
    LogWriteSerial("[CopyVas]: done!\n");
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
    GetCpu()->global_vas_mappings = TreeCreate();
    TreeSetComparator(GetCpu()->global_vas_mappings, VirtAvlComparator);
    ArchInitVirt();

    kernel_vas = GetVas();
    virt_initialised = true;
    
    MarkTfwStartPoint(TFW_SP_AFTER_VIRT);
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
int handling_page_fault = 0;

void HandleVirtFault(size_t faulting_virt, int fault_type) {
    if (GetIrql() >= IRQL_SCHEDULER) {
        PanicEx(PANIC_INVALID_IRQL, "page fault while IRQL >= IRQL_SCHEDULER. is some clown holding a spinlock while "
                                    "executing pageable code? or calling AllocHeapEx wrong with a lock held?");
    }

    struct vas* vas = GetVas();
    AcquireSpinlock(&vas->lock);
    ++handling_page_fault;

    LogWriteSerial("PF at 0x%X on thread 0x%X\n", faulting_virt, GetThread());

    struct vas_entry* entry = GetVirtEntry(vas, faulting_virt);

    if (entry == NULL) {
        UnhandledFault();
    }

    /*
     * TODO: 
     * I've learnt we can fault on reading entry here... The reason? I think it's 
     * because GetVirtEntry() can return something in the global page structure, but
     * between there and here, someone else modifies it...? Maybe we need to look
     * individual entries... and have e.g. GetVirtEntry and ReleaseVirtEntry
     * 
     * It looks like the previous page fault ended up calling `UnhandledFault()` - 
     * it looks like the app stuffed up, but in terminating the app we've somehow 
     * taken down the system, due to the above issue.
     * 
     * page fault: cr2 0x20ADB000, eip 0x1080208F, nos-err 0x4
        PF at 0x20ADB000 on thread 0xC405CCC4
        --> BSS
        Removing fd... file = [0xC405A0EC, 0xC42898EC], fd = 4


        Page fault: cr2 0x8, eip 0x108006D0, nos-err 0x4        
        PF at 0x8 on thread 0xC405CCC4
        unhandled fault...                      <---- user app screws up


        Page fault: cr2 0x5, eip 0xC010560D, nos-err 0x0        <---- this isn't good - this address is the 
                                                                      `entry->load_in_progress` line
        PANIC 12 page fault while IRQL >= IRQL_SCHEDULER. is some clown holding a spinlock while executing pageable code? or calling AllocHeapEx wrong with a lock held?
     */
    if (entry->load_in_progress) {
        LogWriteSerial("Telling a page to retry as loading is already in progress...\n");
        --handling_page_fault;
        ReleaseSpinlock(&vas->lock);
        Schedule();
        return;
    }

    /*
     * Sanity check that our flags are configured correctly.
     */
    assert(!(entry->in_ram && entry->swapfile));
    assert(!(entry->file && entry->swapfile));
    assert(!(!entry->in_ram && entry->lock));
    assert(!(entry->cow && entry->lock));

    // TODO: check for access violations (e.g. user using a supervisor page)
    //          (read / write is not necessarily a problem, e.g. COW)

    int result = BringIntoMemory(vas, entry, fault_type & VM_WRITE, faulting_virt, fault_type);
    if (result != 0) {
        UnhandledFault();
    }

    --handling_page_fault;
    ReleaseSpinlock(&vas->lock);
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

    if (vas == GetVas()) {
        Panic(PANIC_VAS_TRIED_TO_SELF_DESTRUCT);
    }

    // TODO: may need to add reference counting later on (depending on what we need),
    //       just decrement here, and only delete if got to 0.
}