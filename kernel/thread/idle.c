
#include <arch.h>
#include <irql.h>
#include <thread.h>
#include <assert.h>
#include <virtual.h>

void IdleThread(void* ignored) {
    (void) ignored;
    EXACT_IRQL(IRQL_STANDARD);

    SetThreadPriority(GetThread(), SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_IDLE);

    while (1) {
        ArchStallProcessor();
    }
}

void InitIdle(void) {
    CreateThread(IdleThread, NULL, GetVas(), "idle thread");
}