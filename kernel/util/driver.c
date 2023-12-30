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
#include <radixtrie.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>

static struct semaphore* driver_table_lock;
static struct semaphore* symbol_table_lock;
static struct avl_tree* loaded_drivers;
static struct radix_trie* symbol_table;

struct loaded_driver {
    char* filename;
    size_t relocation_point;
    struct quick_relocation_table* quick_relocation_table;
};

static int DriverTableComparatorByRelocationPoint(void* a_, void* b_) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    struct loaded_driver* a = a_;
    struct loaded_driver* b = b_;

    if (a->relocation_point > b->relocation_point) {
        return 1;
    } else if (a->relocation_point < b->relocation_point) {
        return -1;
    } else {
        return 0;
    }
}

static int DriverTableComparatorByName(void* a_, void* b_) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    struct loaded_driver* a = a_;
    struct loaded_driver* b = b_;

    return strcmp(a->filename, b->filename);
}

static size_t GetDriverAddressWithLockHeld(const char* name) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    struct loaded_driver dummy;
    dummy.filename = (char*) name;

    AvlTreeSetComparator(loaded_drivers, DriverTableComparatorByName);
    struct loaded_driver* res = AvlTreeGet(loaded_drivers, &dummy);
    if (res == NULL) {
        return 0;
    }
    return res->relocation_point;
}

static struct loaded_driver* GetDriverFromAddress(size_t relocation_point) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    AcquireMutex(driver_table_lock, -1);

    struct loaded_driver dummy;
    dummy.relocation_point = relocation_point;

    LogWriteSerial("The relocation point is 0x%X\n", relocation_point);

    AvlTreeSetComparator(loaded_drivers, DriverTableComparatorByRelocationPoint);
    struct loaded_driver* res = AvlTreeGet(loaded_drivers, &dummy);
    ReleaseMutex(driver_table_lock);
    return res;
}

size_t GetDriverAddress(const char* name) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    AcquireMutex(driver_table_lock, -1);
    size_t res = GetDriverAddressWithLockHeld(name);
    ReleaseMutex(driver_table_lock);
    return res;
}

void InitSymbolTable(void) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    driver_table_lock = CreateMutex("drv table");
    symbol_table_lock = CreateMutex("sym table");

    loaded_drivers = AvlTreeCreate();
    symbol_table = RadixTrieCreate();

    struct open_file* kernel_file;
    int res = OpenFile("sys:/kernel.exe", O_RDONLY, 0, &kernel_file);
    if (res != 0) {
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
    MAX_IRQL(IRQL_PAGE_FAULT);   

    if (DoesSymbolContainIllegalCharacters(symbol)) {
        return;
    }

    struct long_bool_list b = RadixTrieCreateBoolListFromData64((char*) symbol);
    AcquireMutex(symbol_table_lock, -1);
    RadixTrieInsert(symbol_table, &b, (void*) address);
    ReleaseMutex(symbol_table_lock);
    
    if (GetSymbolAddress(symbol) != address) {
        PanicEx(PANIC_ASSERTION_FAILURE, "Bad radix trie!");
    }
}

size_t GetSymbolAddress(const char* symbol) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    struct long_bool_list b = RadixTrieCreateBoolListFromData64((char*) symbol);

    AcquireMutex(symbol_table_lock, -1);
    void* result = RadixTrieGet(symbol_table, &b);
    ReleaseMutex(symbol_table_lock);

    return (size_t) result;
}

static int LoadDriver(const char* name) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    LogWriteSerial("loading driver...\n");

    struct open_file* file;
    int res = OpenFile(name, O_RDONLY, 0, &file);
    if (res != 0) {
        return res;
    }
    
    struct loaded_driver* drv = AllocHeap(sizeof(struct loaded_driver));
    drv->filename = strdup_pageable(name);
    drv->quick_relocation_table = NULL;
    res = ArchLoadDriver(&drv->relocation_point, file, &drv->quick_relocation_table);
    LogWriteSerial("ArchLoadDriver returned %d\n", res);
    if (res != 0) {
        return res;
    }

    assert(drv->quick_relocation_table != NULL);

    AvlTreeInsert(loaded_drivers, drv);
    ArchLoadSymbols(file, drv->relocation_point - 0xD0000000);
    return 0;
}

int RequireDriver(const char* name) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

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

static int QuickRelocationTableComparator(const void* a_, const void* b_) {
	struct quick_relocation a = *((struct quick_relocation*) a_);
	struct quick_relocation b = *((struct quick_relocation*) b_);

	if (a.address > b.address) {
		return 1;

	} else if (a.address < b.address) {
		return -1;

	} else {
		return 0;
	}
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
    LogWriteSerial("ApplyRelocationsToPage: looking for 0x%X\n", virtual);

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

    bool needs_write_low = (GetVirtPermissions(virtual) & VM_WRITE) == 0;
	bool needs_write_high = (GetVirtPermissions(virtual + sizeof(size_t) - 1) & VM_WRITE) == 0;
	if (needs_write_low) {
		SetVirtPermissions(virtual, VM_WRITE, 0);
	}
	if (needs_write_high) {
		SetVirtPermissions(virtual + sizeof(size_t) - 1, VM_WRITE, 0);
	}

    size_t final_address = table->entries[table->used_entries - 1].address;

    while (entry->address / ARCH_PAGE_SIZE == virtual / ARCH_PAGE_SIZE
        || (entry->address - sizeof(size_t) + 1) == virtual / ARCH_PAGE_SIZE) {

        LogWriteSerial("reapplying relocation: 0x%X -> 0x%X\n", entry->address, entry->value);

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
		SetVirtPermissions(virtual + sizeof(size_t) - 1, 0, VM_WRITE);
	}

}

void PerformDriverRelocationOnPage(struct vas*, size_t relocation_base, size_t virt) {
    LogWriteSerial("PerformDriverRelocationOnPage A\n");
    struct loaded_driver* drv = GetDriverFromAddress(relocation_base);
    if (drv == NULL) {
        PanicEx(PANIC_ASSERTION_FAILURE, "PerformDriverRelocationOnPage");
    }
    LogWriteSerial("PerformDriverRelocationOnPage B. driver at 0x%X\n", drv);

    ApplyRelocationsToPage(drv->quick_relocation_table, virt);
}