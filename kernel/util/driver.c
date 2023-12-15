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
#include <virtual.h>
#include <radixtrie.h>
#include <fcntl.h>
#include <assert.h>

static struct semaphore* driver_table_lock;
static struct semaphore* symbol_table_lock;
static struct avl_tree* loaded_drivers;
static struct radix_trie* symbol_table;

struct loaded_driver {
    char* filename;
    size_t relocation_point;
};

static int DriverTableComparator(void* a_, void* b_) {
    EXACT_IRQL(IRQL_STANDARD);

    struct loaded_driver* a = a_;
    struct loaded_driver* b = b_;

    return strcmp(a->filename, b->filename);
}

static size_t GetDriverAddressWithLockHeld(const char* name) {
    EXACT_IRQL(IRQL_STANDARD);

    struct loaded_driver dummy;
    dummy.filename = (char*) name;

    struct loaded_driver* res = AvlTreeGet(loaded_drivers, &dummy);
    if (res == NULL) {
        return 0;
    }
    return res->relocation_point;
}

size_t GetDriverAddress(const char* name) {
    EXACT_IRQL(IRQL_STANDARD);

    AcquireMutex(driver_table_lock, -1);
    size_t res = GetDriverAddressWithLockHeld(name);
    ReleaseMutex(driver_table_lock);
    return res;
}

void InitSymbolTable(void) {
    EXACT_IRQL(IRQL_STANDARD);

    driver_table_lock = CreateMutex();
    symbol_table_lock = CreateMutex();

    loaded_drivers = AvlTreeCreate();
    AvlTreeSetComparator(loaded_drivers, DriverTableComparator);

    symbol_table = RadixTrieCreate();

    // TODO: load the kernel symbols.
}

void AddSymbol(const char* symbol, size_t address) {
    EXACT_IRQL(IRQL_STANDARD);

    struct long_bool_list b = RadixTrieCreateBoolListFromData64((char*) symbol);

    AcquireMutex(symbol_table_lock, -1);
    RadixTrieInsert(symbol_table, &b, (void*) address);
    ReleaseMutex(symbol_table_lock);
}

size_t GetSymbolAddress(const char* symbol) {
    EXACT_IRQL(IRQL_STANDARD);

    struct long_bool_list b = RadixTrieCreateBoolListFromData64((char*) symbol);

    AcquireMutex(symbol_table_lock, -1);
    void* result = RadixTrieGet(symbol_table, &b);
    ReleaseMutex(symbol_table_lock);

    return (size_t) result;
}

static int LoadDriver(const char* name) {
    EXACT_IRQL(IRQL_STANDARD);

    struct open_file* file;
    int res = OpenFile(name, O_RDONLY, 0, &file);
    if (res != 0) {
        return res;
    }
    
    struct loaded_driver* drv = AllocHeapEx(sizeof(struct loaded_driver), 0);
    drv->filename = strdup_pageable(name);
    res = ArchLoadDriver(&drv->relocation_point, file);
    if (res != 0) {
        return res;
    }

    AvlTreeInsert(loaded_drivers, drv);
    return 0;
}

int RequireDriver(const char* name) {
    EXACT_IRQL(IRQL_STANDARD);

    AcquireMutex(driver_table_lock, -1);

    if (GetDriverAddress(name) != 0) {
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
