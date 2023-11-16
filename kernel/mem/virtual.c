
#include <virtual.h>
#include <avl.h>
#include <heap.h>
#include <common.h>
#include <arch.h>
#include <physical.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <sys/types.h>
#include <irql.h>
#include <cpu.h>

// TODO: spinlocks!

static int VirtAvlComparator(void* a, void* b) {
    struct vas_entry* a_entry = (struct vas_entry*) a;
    struct vas_entry* b_entry = (struct vas_entry*) b;
    if (a_entry->virtual == b_entry->virtual) {
        return 0;
    }
    return (a_entry->virtual < b_entry->virtual) ? -1 : 1;
}

struct vas* CreateVas(void) {
    struct vas* vas = AllocHeap(sizeof(struct vas));
    vas->mappings = AvlTreeCreate();
    AvlTreeSetComparator(vas->mappings, VirtAvlComparator);
    ArchAddGlobalsToVas(vas);
    return vas;
}

void EvictPage(struct vas* vas, struct vas_entry* entry) {
    assert(!entry->lock);
    assert(!entry->cow);

    if (!entry->in_ram) {
        /*
         * Nothing happens, as this page isn't even in RAM.
         */
         
    } else if (entry->file) {
        /*
         * We will just reload it from disk next time.
         */

        // TODO: write entry->virtual back to file.
        
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

        ArchUnmap(vas, entry);
        
        DeallocPhys(entry->physical);
        ArchFlushTlb(vas);
    }
}

void EvictVirt(void) {

}

static void AddMapping(struct vas* vas, size_t physical, size_t virtual, int flags, void* file, off_t pos) {
    struct vas_entry* entry = AllocHeap(sizeof(struct vas_entry));
    entry->allocated = false;

    bool lock = flags & VM_LOCK;
    entry->lock = lock;
    entry->in_ram = lock;

    if (lock) {
        /*
         * We are not allowed to check if the physical page is allocated/free, because it might come
         * from a VM_MAP_HARDWARE request, which can map non-RAM pages. 
         */
        if (physical != 0) {
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
    
    AvlTreeInsert(vas->mappings, entry);
    
    if (entry->in_ram) {
        ArchAddMapping(vas, entry);
    }
}

static bool IsPageInUse(struct vas* vas, size_t virtual) {
    struct vas_entry dummy;
    dummy.virtual = virtual;
    return AvlTreeContains(vas->mappings, (void*) &dummy);
}

static bool IsRangeInUse(struct vas* vas, size_t virtual, size_t pages) {
    for (size_t i = 0; i < pages; ++i) {
        if (IsPageInUse(vas, virtual + i * ARCH_PAGE_SIZE)) {
            return true;
        }
    }

    return false;
}

static size_t AllocVirtRange(size_t pages) {
    (void) pages;
    return 0;
}

static void FreeVirtRange(size_t virtual, size_t pages) {
    (void) virtual;
    (void) pages;
}

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
        AddMapping(vas, physical + i * ARCH_PAGE_SIZE, virtual + i * ARCH_PAGE_SIZE, flags, file, pos);
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
    dummy.virtual = virtual;
    
    struct vas_entry* res = (struct vas_entry*) AvlTreeGet(vas->mappings, (void*) &dummy);
    assert(res != NULL);
    return res;
}

size_t GetPhysFromVirt(size_t virtual) {
    return GetVirtEntry(GetVas(), virtual)->physical;
}

void LockVirt(size_t virtual) {
    struct vas_entry* entry = GetVirtEntry(GetVas(), virtual);
    entry->lock = true;
}

void UnlockVirt(size_t virtual) {
    struct vas_entry* entry = GetVirtEntry(GetVas(), virtual);
    entry->lock = false;
}

void SetVirtPermissions(size_t virtual, int set, int clear) {
    /*
     * Only allow these flags to be set / cleared.
     */
    if ((set | clear) & ~(VM_READ | VM_WRITE | VM_EXEC)) {
        assert(false);
        return;
    }

    struct vas_entry* entry = GetVirtEntry(GetVas(), virtual);
    entry->read = (set & VM_READ) ? true : (clear & VM_READ ? false : entry->read);
    entry->write = (set & VM_WRITE) ? true : (clear & VM_WRITE ? false : entry->write);
    entry->exec = (set & VM_EXEC) ? true : (clear & VM_EXEC ? false : entry->exec);
    entry->user = (set & VM_USER) ? true : (clear & VM_USER ? false : entry->user);

    ArchUpdateMapping(GetVas(), entry);
    ArchFlushTlb(GetVas());
}

int GetVirtPermissions(size_t virtual) {
    struct vas_entry* entry = GetVirtEntry(GetVas(), virtual);
    int permissions = 0;
    if (entry->read) permissions |= VM_READ;
    if (entry->write) permissions |= VM_WRITE;
    if (entry->exec) permissions |= VM_EXEC;
    if (entry->lock) permissions |= VM_LOCK;
    if (entry->file) permissions |= VM_FILE;
    if (entry->user) permissions |= VM_USER;
    return permissions;
}

void UnmapVirt(size_t virtual, size_t bytes) {
    size_t pages = (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;

    bool needs_tlb_flush = false;

    struct vas* vas = GetVas();
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
                 */
            }
            if (entry->file) { 
                /*
                 * TODO: write to the file
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
                Panic(PANIC_NOT_IMPLEMENTED);
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
    CopyVasRecursive(AvlTreeGetRootNode(vas->mappings), new_vas);
    ArchFlushTlb(vas);
    return new_vas;
}

struct vas* GetVas(void) {
    return GetCpu()->current_vas;
}

void SetVas(struct vas* vas) {
    GetCpu()->current_vas = vas;
    ArchSetVas(vas);
}

static bool virt_initialised = false;

void InitVirt(void) {
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

void HandleVirtFault(void* fault_info) {
    int irql = RaiseIrql(IRQL_PAGE_FAULT);

    size_t faulting_virt = ArchGetVirtFaultAddress(fault_info);
    int fault_type = ArchGetVirtFaultType(fault_info);

    struct vas_entry* entry = GetVirtEntry(GetVas(), faulting_virt);
    if (entry == NULL) {
        Panic(PANIC_PAGE_FAULT_IN_NON_PAGED_AREA);
    }

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