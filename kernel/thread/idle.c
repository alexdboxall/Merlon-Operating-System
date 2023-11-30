
#include <arch.h>
#include <irql.h>
#include <thread.h>
#include <assert.h>
#include <virtual.h>

void IdleThread(void* ignored) {
    (void) ignored;
    EXACT_IRQL(IRQL_STANDARD);

    while (1) {
        ArchStallProcessor();
    }
}

void InitIdle(void) {
    CreateThread(IdleThread, NULL, GetVas(), "idle thread");
}