#include <arch.h>
#include <driver.h>
#include <semaphore.h>
#include <irql.h>
#include <avl.h>
#include <string.h>
#include <vfs.h>
#include <errno.h>
#include <heap.h>
#include <panic.h>
#include <log.h>
#include <stdlib.h>
#include <virtual.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>

static struct semaphore* driver_table_lock;
static struct semaphore* symbol_table_lock;
static struct avl_tree* loaded_drivers;
static struct avl_tree* symbol_table;

struct symbol {
    const char* name;
    size_t addr;  
};

struct loaded_driver {
    char* filename;
    size_t relocation_point;
    struct quick_relocation_table* quick_relocation_table;
};

static int SymbolComparator(void* a_, void* b_) {
    struct symbol* a = a_;
    struct symbol* b = b_;
    return strcmp(a->name, b->name);
}

static int DriverTableComparatorByRelocationPoint(void* a_, void* b_) {
    struct loaded_driver* a = a_;
    struct loaded_driver* b = b_;
    return COMPARE_SIGN(a->relocation_point, b->relocation_point);
}

static int DriverTableComparatorByName(void* a_, void* b_) {
    struct loaded_driver* a = a_;
    struct loaded_driver* b = b_;
    return strcmp(a->filename, b->filename);
}

static int QuickRelocationTableComparator(const void* a_, const void* b_) {
	struct quick_relocation a = *((struct quick_relocation*) a_);
	struct quick_relocation b = *((struct quick_relocation*) b_);
    return COMPARE_SIGN(a.address, b.address);
}

static size_t GetDriverAddressWithLockHeld(const char* name) {
    struct loaded_driver dummy = {.filename = (char*) name};

    AvlTreeSetComparator(loaded_drivers, DriverTableComparatorByName);
    struct loaded_driver* res = AvlTreeGet(loaded_drivers, &dummy);
    if (res == NULL) {
        return 0;
    }
    return res->relocation_point;
}

static struct loaded_driver* GetDriverFromAddress(size_t relocation_point) {
    AcquireMutex(driver_table_lock, -1);

    AvlTreeSetComparator(loaded_drivers, DriverTableComparatorByRelocationPoint);
    struct loaded_driver dummy = {.relocation_point = relocation_point};
    struct loaded_driver* res = AvlTreeGet(loaded_drivers, &dummy);

    ReleaseMutex(driver_table_lock);
    return res;
}

size_t GetDriverAddress(const char* name) {
    EXACT_IRQL(IRQL_STANDARD);   

    AcquireMutex(driver_table_lock, -1);
    size_t res = GetDriverAddressWithLockHeld(name);
    ReleaseMutex(driver_table_lock);
    return res;
}

void InitSymbolTable(void) {
    driver_table_lock = CreateMutex("drv table");
    symbol_table_lock = CreateMutex("sym table");

    loaded_drivers = AvlTreeCreate();
    symbol_table = AvlTreeCreate();
    AvlTreeSetComparator(symbol_table, SymbolComparator);

    struct open_file* kernel_file;
    if (OpenFile("sys:/kernel.exe", O_RDONLY, 0, &kernel_file)) {
        Panic(PANIC_NO_FILESYSTEM);
    }
    ArchLoadSymbols(kernel_file, 0);
    CloseFile(kernel_file);
}

static bool DoesSymbolContainIllegalCharacters(const char* symbol) {
    for (int i = 0; symbol[i]; ++i) {
        if (!isalnum(symbol[i]) && symbol[i] != '_') {
            return true;
        }
    }
    return strlen(symbol) == 0;
}

void AddSymbol(const char* symbol, size_t address) {
    EXACT_IRQL(IRQL_STANDARD);   

    if (DoesSymbolContainIllegalCharacters(symbol)) {
        return;
    }

    struct symbol* entry = AllocHeap(sizeof(struct symbol));
    entry->name = strdup(symbol);
    entry->addr = address;

    AcquireMutex(symbol_table_lock, -1);
    if (AvlTreeContains(symbol_table, entry)) {
        /*
         * The kernel has some symbols declared 'static' to file scope, with
         * duplicate names (e.g. in /dev each file has its own 'Stat'). These 
         * get exported for some reason so we end up with duplicate names. We 
         * must ignore these to avoid AVL issues. They are safe to ignore, as
         * they were meant to be 'static' anyway.
         * 
         * TODO: there may be issues if device drivers try to create their own
         * methods with those names (?) e.g. they use the standard template
         * and have their own 'Stat'.
         */
        FreeHeap(entry);
    } else {
        LogWriteSerial("adding symbol %s -> 0x%X\n", symbol, address);
        AvlTreeInsert(symbol_table, entry);
    }
    ReleaseMutex(symbol_table_lock);
}

/*
 * Do not name a (global) function pointer you receive from this the same as the
 * actual function - this may cause issues with duplicate symbols.
 * 
 * e.g. don't do this at global scope:
 * 
 * void (*MyFunc)(void) = GetSymbolAddress("MyFunc");
 * 
 * Instead, do void (*_MyFunc)(void) or something.
 */
size_t GetSymbolAddress(const char* symbol) {
    EXACT_IRQL(IRQL_STANDARD);   

    struct symbol dummy = {.name = symbol};

    AcquireMutex(symbol_table_lock, -1);
    struct symbol* result = AvlTreeGet(symbol_table, &dummy);
    ReleaseMutex(symbol_table_lock);

    if (result == NULL) {
        return 0;

    } else {
        assert(!strcmp(result->name, symbol));
        return result->addr;
    }
}

static int LoadDriver(const char* name) {
    struct open_file* file;
    int res;
    if ((res = OpenFile(name, O_RDONLY, 0, &file))) {
        return res;
    }
    
    struct loaded_driver* drv = AllocHeap(sizeof(struct loaded_driver));
    drv->filename = strdup_pageable(name);
    drv->quick_relocation_table = NULL;
    drv->relocation_point = 0;      // allow driver to be placed anywhere
    if ((res = ArchLoadDriver(&drv->relocation_point, file, &drv->quick_relocation_table, NULL))) {
        return res;
    }

    assert(drv->quick_relocation_table != NULL);

    AvlTreeInsert(loaded_drivers, drv);
    ArchLoadSymbols(file, drv->relocation_point);
    return 0;
}

int RequireDriver(const char* name) {
    EXACT_IRQL(IRQL_STANDARD);   

    LogWriteSerial("Requiring driver: %s\n", name);

    AcquireMutex(driver_table_lock, -1);

    if (GetDriverAddressWithLockHeld(name) != 0) {
        ReleaseMutex(driver_table_lock);

        /*
         * Not an error that it's already loaded - ideally no one should care if it has already been loaded
         * or not. Hence we give 0 (success), not EALREADY.
         */
        return 0;
    }

    int res = LoadDriver(name);
    ReleaseMutex(driver_table_lock);
    return res;
}

void SortQuickRelocationTable(struct quick_relocation_table* table) {
	struct quick_relocation* entries = table->entries;
	qsort_pageable((void*) entries, table->used_entries, sizeof(struct quick_relocation), QuickRelocationTableComparator);
}

void AddToQuickRelocationTable(struct quick_relocation_table* table, size_t addr, size_t val) {
	assert(table->used_entries < table->total_entries);
	table->entries[table->used_entries].address = addr;
	table->entries[table->used_entries].value = val;
	table->used_entries++;
}

struct quick_relocation_table* CreateQuickRelocationTable(int count) {
    struct quick_relocation_table* table = AllocHeap(sizeof(struct quick_relocation_table));
    struct quick_relocation* entries = (struct quick_relocation*) MapVirt(0, 0, count * sizeof(struct quick_relocation), VM_READ | VM_WRITE, NULL, 0);
    table->entries = entries;
    table->used_entries = 0;
    table->total_entries = count;
    return table;
}

static int BinarySearchComparator(const void* a_, const void* b_) {
    struct quick_relocation a = *((struct quick_relocation*) a_);
	struct quick_relocation b = *((struct quick_relocation*) b_);

    size_t page_a = a.address / ARCH_PAGE_SIZE;
    size_t page_b = b.address / ARCH_PAGE_SIZE;

    if (page_a > page_b) {
        return 1;
    
    } else if (page_a < page_b) {
        return -1;

    } else {
        return 0;
    }
}

static void ApplyRelocationsToPage(struct quick_relocation_table* table, size_t virtual) {
    struct quick_relocation target;
    target.address = virtual;

    struct quick_relocation* entry = bsearch(&target, table->entries, table->used_entries, sizeof(struct quick_relocation), BinarySearchComparator);
    if (entry == NULL) {
        PanicEx(PANIC_ASSERTION_FAILURE, "quick relocation table doesn't contain lookup - bsearch or qsort is probably bugged");
    }

    /*
     * As we only did a binary search to look for anything on that page, we might be halfway through the page!
     * Move back through the entries until we find the start of page (or the first entry in the table).
     */
    while ((entry->address / ARCH_PAGE_SIZE == virtual / ARCH_PAGE_SIZE
        || (entry->address - sizeof(size_t) + 1) == virtual / ARCH_PAGE_SIZE) && entry != table->entries) {
        entry -= 1;
    }

    /*
     * We went past the last one, so need to forward onto it again - unless we hit the start of the table.
     */
    if (entry != table->entries) {
        entry += 1;
    }


    /*
     * We also need to lock these, as (and YES this has actually happened before):
     *   - if the relocation is on the boundary of the next one, and the next one is not present
     *   - and the next one is relocatable
     * Then:
     *   - we make the next page writable
     *   - we try to do the straddle relocation
     *   - that causes a fault on the next page (not present)
     *   - that one is also relocatable, so it enables writing on that page
     *   - it finishes, and unmarks it as writable
     *   - we fail to do the relocation, as it is no longer writable
     * 
     * By locking it first, we force the relocations on the second page to happen first, and then we can
     * mark it as writable.
     */

    bool needs_write_low = (GetVirtPermissions(virtual) & VM_WRITE) == 0;
    bool needs_write_high = false;
    bool need_unlock_high = false;
    
	if (needs_write_low) {
    	SetVirtPermissions(virtual, VM_WRITE, 0);
	}

    size_t final_address = table->entries[table->used_entries - 1].address;

    while (entry->address / ARCH_PAGE_SIZE == virtual / ARCH_PAGE_SIZE
        || (entry->address - sizeof(size_t) + 1) == virtual / ARCH_PAGE_SIZE) {

        if ((entry->address + sizeof(size_t) + 1) / ARCH_PAGE_SIZE != virtual / ARCH_PAGE_SIZE) {
            need_unlock_high = !LockVirt(virtual + ARCH_PAGE_SIZE);
            needs_write_high = (GetVirtPermissions(virtual + ARCH_PAGE_SIZE) & VM_WRITE) == 0;
            if (needs_write_high) {
                SetVirtPermissions(virtual + ARCH_PAGE_SIZE, VM_WRITE, 0);
            }
        }

        size_t* ref = (size_t*) entry->address;
        *ref = entry->value;

        if (entry->address == final_address) {
            break;
        }
        entry += 1;
    }

    if (needs_write_low) {
		SetVirtPermissions(virtual, 0, VM_WRITE);
	}
	if (needs_write_high) {
		SetVirtPermissions(virtual + ARCH_PAGE_SIZE, 0, VM_WRITE);
	}
    if (need_unlock_high) {
        UnlockVirt(virtual + ARCH_PAGE_SIZE);
    }
}

void PerformRelocationsOnPage(struct vas*, size_t relocation_base, size_t virt) {
    struct loaded_driver* drv = GetDriverFromAddress(relocation_base);
    if (drv == NULL) {
        PanicEx(PANIC_ASSERTION_FAILURE, "PerformRelocationsOnPage");
    }

    ApplyRelocationsToPage(drv->quick_relocation_table, virt);
}