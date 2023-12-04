
#include <arch.h>
#include <irql.h>
#include <thread.h>
#include <assert.h>
#include <virtual.h>

/**
 * A thread that can be run if no other thread is available to run. This thread must never terminate or
 * block. Ideally, it should try to invoke power-saving features or perform optimsiations to the system.
 * 
 * @note EXACT_IRQL(IRQL_STANDARD)
 */
static void IdleThread(void*) {
    EXACT_IRQL(IRQL_STANDARD);

    SetThreadPriority(GetThread(), SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_IDLE);

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

    CreateThread(IdleThread, NULL, GetVas(), "idle thread");
}