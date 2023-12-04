#pragma once

#include <common.h>

enum {
    PANIC_UNKNOWN,

    /* 
     * A non-returnable function or infinite loop was exited out of.
     */
    PANIC_IMPOSSIBLE_RETURN,

    /*
     * The panic was requested by the debugger.
     */
    PANIC_MANUALLY_INITIATED,

    /*
     * A unit test succeeded. Only to be used with the unit testing framework,
     * which panics to either succeed or fail (via assertion fails).
     */
    PANIC_UNIT_TEST_OK,

    /*
     * Used by drivers to report unrecoverable faults.
     */
    PANIC_DRIVER_FAULT,

    /*
     * The kernel heap is out of memory and cannot request any more.
     */
    PANIC_OUT_OF_HEAP,

    /*
     * Too much heap memory has been allocated before the virtual memory manager has
     * been initialised.
     */
    PANIC_OUT_OF_BOOTSTRAP_HEAP,

    /*
     * A request for a block on the heap was too large.
     */
    PANIC_HEAP_REQUEST_TOO_LARGE,

    /*
     * The kernel or a driver has caused an illegal page fault.
     */
    PANIC_PAGE_FAULT_IN_NON_PAGED_AREA,

    /*
     * An assertion failure within the kernel or driver.
     */
    PANIC_ASSERTION_FAILURE,

    /*
     * The bootloader failed to provide a usable memory map.
     */
    PANIC_NO_MEMORY_MAP,

    /*
     * The given section of kernel code is not implemented yet.
     */
    PANIC_NOT_IMPLEMENTED,

    /*
     * Wrong IRQL
     */
    PANIC_INVALID_IRQL,

    /*
     * Spinlock acquired from the wrong IRQL level.
     */
    PANIC_SPINLOCK_WRONG_IRQL,

    /*
     * No more physical memory, even after evicting old pages.
     */
    PANIC_OUT_OF_PHYS,

    PANIC_PRIORITY_QUEUE,
    PANIC_LINKED_LIST,

    /*
     * Kernel stack overflow
     */
    PANIC_CANARY_DIED,

    PANIC_SEMAPHORE_DESTROY_WHILE_HELD,
    PANIC_SEM_BLOCK_WITHOUT_THREAD,
    PANIC_CANNOT_LOCK_MEMORY,
    PANIC_THREAD_LIST,
    PANIC_CANNOT_MALLOC_WITHOUT_FAULTING,

    _PANIC_HIGHEST_VALUE
};

export _Noreturn void PanicEx(int code, const char* message);
export _Noreturn void Panic(int code);
