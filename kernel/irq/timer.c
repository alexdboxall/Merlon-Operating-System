
#include <timer.h>
#include <spinlock.h>
#include <assert.h>
#include <cpu.h>
#include <irql.h>
#include <log.h>
#include <arch.h>

static struct spinlock timer_lock;

static uint64_t system_time = 0;

void ReceivedTimer(uint64_t nanos) {
    EXACT_IRQL(IRQL_TIMER);

    if (ArchGetCurrentCpuIndex() == 0) {
        /*
         * As we're in the timer handler, we know we already have IRQL_TIMER, and so we don't
         * need to incur the additional overhead of raising and lowering.
         */
        AcquireSpinlock(&timer_lock, false);
        system_time += nanos;
        ReleaseSpinlock(&timer_lock);
    }
}

uint64_t GetSystemTimer(void) {
    MAX_IRQL(IRQL_TIMER);

    int irql = AcquireSpinlock(&timer_lock, true);
    uint64_t value = system_time;
    ReleaseSpinlockAndLower(&timer_lock, irql);
    return value;
}

void InitTimer(void) {
    InitSpinlock(&timer_lock, "timer", IRQL_TIMER);
}   