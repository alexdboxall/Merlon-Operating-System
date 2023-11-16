
#include <irql.h>
#include <assert.h>

void ScheduleInternal(void) {
    assert(GetIrql() == IRQL_SCHEDULER);
    // TODO: assert(spinlock is held)

    
}

void Schedule(void) {
    if (GetIrql() >= IRQL_SCHEDULER) {
        // TODO: defer it
        return;
    }

    int irql = RaiseIrql(IRQL_SCHEDULER);
    // TODO: acquire scheduler spinlock
    ScheduleInternal();
    // TODO: release scheduler spinlock
    LowerIrql(irql);
}