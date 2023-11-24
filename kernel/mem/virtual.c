
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

static bool virt_initialised = false;

static int VirtAvlComparator(void* a, void* b) {
    struct vas_entry* a_entry = (struct vas_entry*) a;
    struct vas_entry* b_entry = (struct vas_entry*) b;
    if (a_entry->virtual == b_entry->virtual) {
        return 0;
    }
    return (a_entry->virtual < b_entry->virtual) ? -1 : 1;
}

void CreateVasEx(struct vas* vas, int flags) {
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

struct vas* CreateVas() {
    struct vas* vas = AllocHeap(sizeof(struct vas));
    CreateVasEx(vas, 0);
    return vas;
}

void EvictPage(struct vas* vas, struct vas_entry* entry) {
    assert(!entry->lock);
    assert(!entry->cow);

    int irql = AcquireSpinlock(&vas->lock, true);

    if (!entry->in_ram) {
        /*
         * Nothing happens, as this page isn't even in RAM.
         */
         
    } else if (entry->file) {
        /*
         * We will just reload it from disk next time.
         */

        // TODO: write entry->virtual back to file.
        //       this will probably want to be deferred (i.e. copy the data to get rid of to a 
        //       new AllocPhys() page, then give that to the defer handler.

        entry->in_ram = false;
        DeallocPhys(entry->physical);
        ArchUnmap(vas, entry);
        ArchFlushTlb(vas);

    } else {
        /*
         * Otherwise, we need to mark it as swapfile.
         */
        entry->in_ram = false;
        entry->swapfile = true;


        // TODO: put it on the swapfile!
        //       this will probably want to be deferred (i.e. copy the data to get rid of to a 
        //       new AllocPhys() page, then give that to the defer handler.

        ArchUnmap(vas, entry);
        DeallocPhys(entry->physical);
        ArchFlushTlb(vas);
    }
    
    ReleaseSpinlockAndLower(&vas->lock, irql);
}

void EvictVirt(void) {

}

static void AddMapping(struct vas* vas, size_t physical, size_t virtual, int flags, void* file, off_t pos) {
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
    entry->read = flags & VM_READ;
    entry->write = flags & VM_WRITE;
    entry->exec = flags & VM_EXEC;
    entry->file = flags & VM_FILE;
    entry->physical = physical;
    entry->ref_count = 1;
    entry->file_offset = pos;
    entry->file_node = file;

    /*
     * TODO: later on, check if shared, and add phys->virt entry if needed
     */
    
    int irql = AcquireSpinlock(&vas->lock, true);
    AvlTreeInsert(vas->mappings, entry);
    ArchAddMapping(vas, entry);
    ReleaseSpinlockAndLower(&vas->lock, irql);
}

static bool IsRangeInUse(struct vas* vas, size_t virtual, size_t pages) {
    bool in_use = false;

    struct vas_entry dummy;
    dummy.virtual = virtual;

    int irql = AcquireSpinlock(&vas->lock, true);
    for (size_t i = 0; i < pages; ++i) {
        if (AvlTreeContains(vas->mappings, (void*) &dummy)) {
            in_use = true;
            break;
        }
        dummy.virtual += ARCH_PAGE_SIZE;
    }

    ReleaseSpinlockAndLower(&vas->lock, irql);
    return in_use;
}

static size_t AllocVirtRange(size_t pages) {
    /*
     * TODO: make this deallocatable, and not x86 specific (with that memory address)
     * TODO: this probably needs a global lock of some sort.
     */
    static size_t hideous_allocator = 0xD0000000U;
    size_t retv = hideous_allocator;
    hideous_allocator += pages * ARCH_PAGE_SIZE;
    return retv;
}

static void FreeVirtRange(size_t virtual, size_t pages) {
    (void) virtual;
    (void) pages;
}

/*
static void AvlPrinter(void* data_) {
    struct vas_entry* data = (struct vas_entry*) data_;
    LogWriteSerial("[v: 0x%X, p: 0x%X; acrl: %d%d%d%d. ref: %d]; ", data->virtual, data->physical, data->allocated, data->cow, data->in_ram, data->lock, data->ref_count);
}*/

static size_t MapVirtEx(struct vas* vas, size_t physical, size_t virtual, size_t pages, int flags, void* file, off_t pos) {
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

    if ((flags & VM_MAP_HARDWARE) && !(flags & VM_LOCK)) {
        return 0;
    }

    if ((flags & VM_FILE) && file == NULL) {
        return 0;
    }
    
    if (!(flags & VM_FILE) && (file != NULL || pos != 0)) {
        return 0;
    }

    /*
     * Get a virtual memory range that is not currently in use.
     */
    if (virtual == 0) {
        virtual = AllocVirtRange(pages);

    } else {
        if (IsRangeInUse(vas, virtual, pages)) {
            if (flags & VM_FIXED_VIRT) {
                return 0;
            }

            virtual = AllocVirtRange(pages);
        }
    }
    
    for (size_t i = 0; i < pages; ++i) {
        AddMapping(vas, physical == 0 ? 0 : (physical + i * ARCH_PAGE_SIZE), virtual + i * ARCH_PAGE_SIZE, flags, file, pos);
    }   

    if (vas == GetVas()) {
        ArchFlushTlb(vas);
    }
    
    return virtual;
}

size_t MapVirt(size_t physical, size_t virtual, size_t bytes, int flags, void* file, off_t pos) {
    size_t pages = (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
    return MapVirtEx(GetVas(), physical, virtual, pages, flags, file, pos);
}

static struct vas_entry* GetVirtEntry(struct vas* vas, size_t virtual) {
    struct vas_entry dummy;
#ifndef NDEBUG   
    dummy.physical = 0xBAADC0DE;
#endif
    dummy.virtual = virtual;

    // TODO: assert lock is held.

    struct vas_entry* res = (struct vas_entry*) AvlTreeGet(vas->mappings, (void*) &dummy);
    assert(res != NULL);
    return res;
}

size_t GetPhysFromVirt(size_t virtual) {
    struct vas* vas = GetVas();
    int irql = AcquireSpinlock(&vas->lock, true);
    size_t result = GetVirtEntry(GetVas(), virtual)->physical;
    ReleaseSpinlockAndLower(&vas->lock, irql);
    return result;
}

void LockVirt(size_t virtual) {
    struct vas* vas = GetVas();
    int irql = AcquireSpinlock(&vas->lock, true);

    struct vas_entry* entry = GetVirtEntry(vas, virtual);

    if (!entry->in_ram) {
        // TODO: probably need to make HandleVirtFault call a subfunction 'BringIntoRAM', and then have
        //       that be called here too.
    }

    entry->lock = true;
    ReleaseSpinlockAndLower(&vas->lock, irql);
}

void UnlockVirt(size_t virtual) {
    struct vas* vas = GetVas();
    int irql = AcquireSpinlock(&vas->lock, true);
    struct vas_entry* entry = GetVirtEntry(vas, virtual);
    entry->lock = false;
    ReleaseSpinlockAndLower(&vas->lock, irql);
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
    int irql = AcquireSpinlock(&vas->lock, true);

    struct vas_entry* entry = GetVirtEntry(vas, virtual);
    entry->read = (set & VM_READ) ? true : (clear & VM_READ ? false : entry->read);
    entry->write = (set & VM_WRITE) ? true : (clear & VM_WRITE ? false : entry->write);
    entry->exec = (set & VM_EXEC) ? true : (clear & VM_EXEC ? false : entry->exec);
    entry->user = (set & VM_USER) ? true : (clear & VM_USER ? false : entry->user);

    ArchUpdateMapping(vas, entry);
    ArchFlushTlb(vas);

    ReleaseSpinlockAndLower(&vas->lock, irql);
}

int GetVirtPermissions(size_t virtual) {
    struct vas* vas = GetVas();
    int irql = AcquireSpinlock(&vas->lock, true);
    struct vas_entry entry = *GetVirtEntry(GetVas(), virtual);
    ReleaseSpinlockAndLower(&vas->lock, irql);

    int permissions = 0;
    if (entry.read) permissions |= VM_READ;
    if (entry.write) permissions |= VM_WRITE;
    if (entry.exec) permissions |= VM_EXEC;
    if (entry.lock) permissions |= VM_LOCK;
    if (entry.file) permissions |= VM_FILE;
    if (entry.user) permissions |= VM_USER;

    return permissions;
}

void UnmapVirt(size_t virtual, size_t bytes) {
    size_t pages = (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
    bool needs_tlb_flush = false;

    struct vas* vas = GetVas();

    int irql = AcquireSpinlock(&vas->lock, true);

    for (size_t i = 0; i < pages; ++i) {
        struct vas_entry* entry = GetVirtEntry(vas, virtual + i * ARCH_PAGE_SIZE);
        entry->ref_count--;

        if (entry->ref_count == 0) {
            if (entry->in_ram) {
                ArchUnmap(vas, entry);
                needs_tlb_flush = true;
            }
            if (entry->swapfile) {
                /*
                 * TODO: there will eventually something that indicates where on the swapfile
                 *       things are free/allocated. basically here, we just have to mark it as
                 *       free on the disk again (don't need to clear the actual data or anything)
                 * 
                 *       remember to defer it
                 */
            }
            if (entry->file) { 
                /*
                 * TODO: write to the file
                 *
                 *       remember to defer it
                 */
            }
            if (entry->allocated) {
                // TODO: what if it's on the swapfile?
                DeallocPhys(entry->physical);
            }

            AvlTreeDelete(vas->mappings, entry); 
            FreeHeap(entry);
            FreeVirtRange(virtual + i * ARCH_PAGE_SIZE, 1);
        }
    }

    if (needs_tlb_flush) {
        ArchFlushTlb(vas);
    }

    ReleaseSpinlockAndLower(&vas->lock, irql);
}

static void CopyVasRecursive(struct avl_node* node, struct vas* new_vas) {
    if (node == NULL) {
        return;
    }

    CopyVasRecursive(AvlTreeGetLeft(node), new_vas);
    CopyVasRecursive(AvlTreeGetRight(node), new_vas);

    struct vas_entry* entry = AvlTreeGetData(node);

    /*
     * Kernel global entries have already been done in the new VAS by CreateVas(), so
     * no need to copy them again.
     */
    if (!entry->global) {
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
                memcpy(page_data, (void*) entry->virtual, ARCH_PAGE_SIZE);
                size_t new_physical = entry->physical;
                entry->physical = AllocPhys();
                ArchUpdateMapping(GetVas(), entry);
                ArchFlushTlb(GetVas());
                memcpy((void*) entry->virtual, page_data, ARCH_PAGE_SIZE);

                struct vas_entry* new_entry = AllocHeap(sizeof(struct vas_entry));
                *new_entry = *entry;
                new_entry->ref_count = 1;
                new_entry->physical = new_physical;
                new_entry->allocated = true;
                AvlTreeInsert(new_vas->mappings, entry);
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

            AvlTreeInsert(new_vas->mappings, entry);

            ArchUpdateMapping(GetVas(), entry);
            ArchAddMapping(new_vas, entry);
        }
    }
}

struct vas* CopyVas(void) {
    struct vas* vas = GetVas();
    struct vas* new_vas = CreateVas();

    int irql = AcquireSpinlock(&vas->lock, true);
    CopyVasRecursive(AvlTreeGetRootNode(vas->mappings), new_vas);
    ArchFlushTlb(vas);
    ReleaseSpinlockAndLower(&vas->lock, irql);

    return new_vas;
}

struct vas* GetVas(void) {
    // TODO: cpu probably needs to have a lock object in it called current_vas_lock, which needs to be held whenever
    //       someone reads or writes to current_vas;
    return GetCpu()->current_vas;
}

void SetVas(struct vas* vas) {
    GetCpu()->current_vas = vas;
    ArchSetVas(vas);
}

void InitVirt(void) {
    // TODO: cpu probably needs to have a lock object in it called current_vas_lock, which needs to be held whenever
    //       someone reads or writes to current_vas;

    assert(!virt_initialised);
    ArchInitVirt();
    virt_initialised = true;   
}

static void HandleCowFault(struct vas_entry* entry) {
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
    memcpy(page_data, (void*) entry->virtual, ARCH_PAGE_SIZE);

    entry->ref_count--;

    if (entry->ref_count == 1) {
        entry->cow = false;
    }
        
    struct vas_entry* new_entry = AllocHeap(sizeof(struct vas_entry));
    *new_entry = *entry;
    new_entry->ref_count = 1;
    new_entry->physical = AllocPhys();
    new_entry->allocated = true;
    AvlTreeDelete(GetVas()->mappings, entry);
    FreeHeap(entry);
    ArchUpdateMapping(GetVas(), entry);
    ArchFlushTlb(GetVas());
    memcpy((void*) entry->virtual, page_data, ARCH_PAGE_SIZE);
}

static void HandleFileBackedFault(struct vas_entry* entry) {
    (void) entry;
}

static void HandleSwapfileFault(struct vas_entry* entry) {
    entry->physical = AllocPhys();
    entry->allocated = true;
    entry->in_ram = true;
    entry->swapfile = false;
    ArchUpdateMapping(GetVas(), entry);
    ArchFlushTlb(GetVas());

    // TODO: read into entry->virtual from swapfile
}

void HandleVirtFault(size_t faulting_virt, int fault_type) {
    LogWriteSerial("HandleVirtFault A\n");
    int irql = RaiseIrql(IRQL_PAGE_FAULT);

    (void) fault_type;

    struct vas_entry* entry = GetVirtEntry(GetVas(), faulting_virt);
    if (entry == NULL) {
        Panic(PANIC_PAGE_FAULT_IN_NON_PAGED_AREA);
    }
    LogWriteSerial("\nentry: v 0x%X, p 0x%X. aclr = %d%d%d%d\n", entry->virtual, entry->physical, entry->allocated, entry->cow, entry->lock, entry->in_ram);
    LogWriteSerial("HandleVirtFault B\n");

    /*
     * Sanity check that our flags are configured correctly.
     */
    assert(!(entry->in_ram && entry->swapfile));
    assert(!(entry->file && entry->swapfile));
    assert(!(!entry->in_ram && entry->lock));
    assert(!(entry->cow && entry->lock));
    assert(!(entry->cow && entry->global));

    // TODO: check for access violations (e.g. user using a supervisor page)
    //          (read / write is not necessarily a problem, e.g. COW)

    if (entry->cow && (fault_type & VM_WRITE)) {
        HandleCowFault(entry);
        LowerIrql(irql);
        return;
    }

    if (entry->file && !entry->in_ram) {
        HandleFileBackedFault(entry);
        LowerIrql(irql);
        return;
    }

    if (entry->swapfile) {
        HandleSwapfileFault(entry);
        LowerIrql(irql);
        return;
    }

    // TODO: otherwise, as an entry exists, it just needs to be allocated-on-read/write (the default) and cleared to zero
    //       (it's not a non-paged area, as an entry for it exists)
    if (!entry->in_ram) {
        entry->physical = AllocPhys();
        entry->allocated = true;
        entry->in_ram = true;
        ArchUpdateMapping(GetVas(), entry);
        ArchFlushTlb(GetVas());
        memset((void*) entry->virtual, 0, ARCH_PAGE_SIZE);

        LowerIrql(irql);
        return;
    }

    Panic(PANIC_UNKNOWN);
}

bool IsVirtInitialised(void) {
    return virt_initialised;
}