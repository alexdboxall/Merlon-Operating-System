
#include <panic.h>
#include <cpu.h>
#include <irql.h>

struct irql_deferment {
    int irql;
    void (*handler)(void*);
    void* context;
};

/*
 * Assumes that interrupts are completely off as we go into this function. 
 */
static void SetHardwareStateForLevel(int level) {
    if (level >= IRQL_DRIVER) {
        // TODO: mask certain irqs
    } else {
        // TODO: unmask all irqs
    }

    if (level < IRQL_PANIC) {
        ArchEnableInterrupts();
    }
}

/**
 * Runs a function at an IRQL lower than or equal to the current IRQL. If the IRQLs match,
 * the function will be run immediately. If the target IRQL is lower than the current IRQL,
 * it will be deferred until the IRQL level drops below IRQL_SCHEDULER. Deferred function
 * calls will be run in order from highest IRQL to lowest.
 *
 * @param irql      The IRQL level to run the function at. This IRQL must be lower than the
 *                  current IRQL, or a panic will occur.
 * @param handler   The function to be run.
 * @param context   An arguement given to the handler function.
 * 
 * @max_irql IRQL_HIGH
 */
void RunAtIrql(int irql, void(*handler)(void*), void* context) {
    if (irql == GetIrql()) {
        handler(context);
        return;

    } else if (irql > GetIrql()) {
        Panic(PANIC_INVALID_IRQL);
    }
}

/**
 * Returns the CPU's current IRQL.
 *
 * @return The IRQL.
 * @max_irql IRQL_HIGH
 */
int GetIrql(void) {
    return GetCpu()->irql;
}

// Max IRQL: IRQL_HIGH
int RaiseIrql(int level) {
    ArchDisableInterrupts();

    int existing_level = GetIrql();

    if (level < existing_level) {
        Panic(PANIC_INVALID_IRQL);
    }

    GetCpu()->irql = level;
    SetHardwareStateForLevel(level);

    return existing_level;
}

// Max IRQL: IRQL_HIGH
void LowerIrql(int level) {
    ArchDisableInterrupts();

    if (level > GetIrql()) {
        Panic(PANIC_INVALID_IRQL);
    }

    /* 
     * Must do these as we leave, not enter IRQL_SCHEDULER, as if e.g. the timer causes a deferred task switch
     * while we are already in Schedule() (i.e. at IRQL_SCHEDULER), we want the orignal task switch to occur
     * before the deferred one gets run (otherwise we corrupt the task switch). 
     */
    if (level < IRQL_SCHEDULER) {
        // TODO: need to go through the deferred heap and go through them in order (obviously setting GetCpu()->irql and
        // SetHardwareStateForLevel as we go, then we do the final switch
    }
   
    GetCpu()->irql = level;
    SetHardwareStateForLevel(level);
}