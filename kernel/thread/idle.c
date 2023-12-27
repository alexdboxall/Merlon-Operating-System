
#include <arch.h>
#include <irql.h>
#include <thread.h>
#include <assert.h>
#include <virtual.h>

/**
 * A thread that can be run if no other thread is available to run. This thread must never terminate or
 * block. Ideally, it should try to invoke power-saving features or perform optimsiations to the system.
 * May be run at IRQL_PAGE_FAULT at some points (usually during boot) where we might be waiting on the disk
 * to pause for a momenet in the page fault handler (this invoking sleep and blocking the task, leaving only
 * the idle task).
 * 
 * @note MAX_IRQL(IRQL_PAGE_FAULT)
 */
static void IdleThread(void*) {
    MAX_IRQL(IRQL_PAGE_FAULT);

    while (1) {
        ArchStallProcessor();
    }
}

/**
 * Adds the idle thread to the list of tasks. This must be called before blocking is guaranteed to work
 * correctly, and therefore should be added before multitasking is initialised.
 * 
 * @note MAX_IRQL(IRQL_SCHEDULER)
 */
void InitIdle(void) {
    MAX_IRQL(IRQL_SCHEDULER);

    CreateThreadEx(IdleThread, NULL, GetVas(), "idle thread", NULL, SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_IDLE, 0);
}