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
};

static int PAGEABLE_CODE_SECTION DriverTableComparator(void* a_, void* b_) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    struct loaded_driver* a = a_;
    struct loaded_driver* b = b_;

    return strcmp(a->filename, b->filename);
}

static size_t PAGEABLE_CODE_SECTION GetDriverAddressWithLockHeld(const char* name) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    struct loaded_driver dummy;
    dummy.filename = (char*) name;

    struct loaded_driver* res = AvlTreeGet(loaded_drivers, &dummy);
    if (res == NULL) {
        return 0;
    }
    return res->relocation_point;
}

size_t PAGEABLE_CODE_SECTION GetDriverAddress(const char* name) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    AcquireMutex(driver_table_lock, -1);
    size_t res = GetDriverAddressWithLockHeld(name);
    ReleaseMutex(driver_table_lock);
    return res;
}

void PAGEABLE_CODE_SECTION InitSymbolTable(void) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    driver_table_lock = CreateMutex("drv table");
    symbol_table_lock = CreateMutex("sym table");
    LogWriteSerial("driver / symbol locks = 0x%X, 0x%X\n", driver_table_lock, symbol_table_lock);

    loaded_drivers = AvlTreeCreate();
    AvlTreeSetComparator(loaded_drivers, DriverTableComparator);

    symbol_table = RadixTrieCreate();

    struct open_file* kernel_file;
    int res = OpenFile("sys:/kernel.exe", O_RDONLY, 0, &kernel_file);
    if (res != 0) {
        Panic(PANIC_NO_FILESYSTEM);
    }
    LogWriteSerial("about to load the kernel symbols...\n");
    ArchLoadSymbols(kernel_file, 0);
    CloseFile(kernel_file);
}

static bool PAGEABLE_CODE_SECTION DoesSymbolContainIllegalCharacters(const char* symbol) {
    for (int i = 0; symbol[i]; ++i) {
        if (!isalnum(symbol[i]) && symbol[i] != '_') {
            return true;
        }
    }
    return strlen(symbol) == 0;
}

void PAGEABLE_CODE_SECTION AddSymbol(const char* symbol, size_t address) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    if (DoesSymbolContainIllegalCharacters(symbol)) {
        return;
    }

    LogWriteSerial("adding symbol %s -> 0x%X\n", symbol, address);

    struct long_bool_list b = RadixTrieCreateBoolListFromData64((char*) symbol);
    AcquireMutex(symbol_table_lock, -1);
    RadixTrieInsert(symbol_table, &b, (void*) address);
    ReleaseMutex(symbol_table_lock);
    
    if (GetSymbolAddress(symbol) != address) {
        PanicEx(PANIC_ASSERTION_FAILURE, "Bad radix trie!");
    }
}

size_t PAGEABLE_CODE_SECTION GetSymbolAddress(const char* symbol) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    struct long_bool_list b = RadixTrieCreateBoolListFromData64((char*) symbol);

    AcquireMutex(symbol_table_lock, -1);
    void* result = RadixTrieGet(symbol_table, &b);
    ReleaseMutex(symbol_table_lock);

    return (size_t) result;
}

static int PAGEABLE_CODE_SECTION LoadDriver(const char* name) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    LogWriteSerial("loading driver...\n");

    struct open_file* file;
    int res = OpenFile(name, O_RDONLY, 0, &file);
    if (res != 0) {
        return res;
    }

    LogWriteSerial("opened file...\n");
    
    struct loaded_driver* drv = AllocHeapEx(sizeof(struct loaded_driver), 0);
    drv->filename = strdup_pageable(name);
    LogWriteSerial("about to ArchLoadDriver...\n");
    res = ArchLoadDriver(&drv->relocation_point, file);
    LogWriteSerial("ArchLoadDriver returned %d\n", res);
    if (res != 0) {
        return res;
    }

    AvlTreeInsert(loaded_drivers, drv);
    ArchLoadSymbols(file, drv->relocation_point - 0xD0000000);
    return 0;
}

int PAGEABLE_CODE_SECTION RequireDriver(const char* name) {
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
