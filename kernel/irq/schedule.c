
#include <irql.h>
#include <common.h>
#include <assert.h>

void ScheduleInternal(void) {
    assert(GetIrql() == IRQL_SCHEDULER);
    // TODO: assert(spinlock is held)

    
}

void Schedule(void) {
    // TODO: decide if a page fault handler should allow task switches or not (I would guess )
    if (GetIrql() != IRQL_STANDARD) {
        PostponeScheduleUntilStandardIrql();
        return;
    }

    int irql = RaiseIrql(IRQL_SCHEDULER);
    // TODO: acquire scheduler spinlock
    ScheduleInternal();
    // TODO: release scheduler spinlock
    LowerIrql(irql);
}