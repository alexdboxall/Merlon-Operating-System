
#include <panic.h>
#include <cpu.h>
#include <irql.h>
#include <log.h>
#include <priorityqueue.h>
#include <schedule.h>

struct irql_deferment {
    void (*handler)(void*);
    void* context;
};

// TODO: needs to be per-cpu
struct priority_queue* deferred_functions;

// TODO: probably needs to be per-cpu??
static bool init_irql_done = false;

/**
 * Runs a function at an IRQL lower than or equal to the current IRQL. If the IRQLs match,
 * the function will be run immediately. If the target IRQL is lower than the current IRQL,
 * it will be deferred until the IRQL level drops below IRQL_SCHEDULER. Deferred function
 * calls will be run in order from highest IRQL to lowest. If InitIrql() has not been called
 * prior to calling this function, any requests meant to be deferred will instead be silently
 * ignored (this is needed to bootstrap the physical memory manager, et al.).
 *
 * @param irql      The IRQL level to run the function at. This IRQL must be lower than the
 *                  current IRQL, or a panic will occur.
 * @param handler   The function to be run.
 * @param context   An arguement given to the handler function.
 * 
 * @max_irql IRQL_HIGH
 */
void DeferUntilIrql(int irql, void(*handler)(void*), void* context) {
    if (irql == GetIrql()) {
        handler(context);

    } else if (irql > GetIrql()) {
        Panic(PANIC_INVALID_IRQL);

    } else {
        if (init_irql_done) {
            struct irql_deferment deferment;
            deferment.context = context;
            deferment.handler = handler;
            PriorityQueueInsert(deferred_functions, (void*) &deferment, irql);
        }
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
    // ArchSetIrql(current_level);

    return existing_level;
}

static bool postponed_task_switch = false;

// Max IRQL: IRQL_HIGH
void LowerIrql(int target_level) {
    // TODO: does this function need its own lock ? (e.g. for postponed_task_switch)
    ArchDisableInterrupts();

    int current_level = GetIrql();

    if (target_level > current_level) {
        Panic(PANIC_INVALID_IRQL);
    }

    while (current_level != target_level) {
        --current_level;
        GetCpu()->irql = current_level;
        // ArchSetIrql(current_level);

        // RUN ANY HANDLERS AT LEVEL
        //      (i.e. look through deferred_tasks[current_level])
    }
   
    if (current_level == IRQL_STANDARD && postponed_task_switch) {
        postponed_task_switch = false;
        Schedule();
    }
}

void PostponeScheduleUntilStandardIrql(void) {
    // TODO: does this function need its own lock ? (e.g. just for setting postponed_task_switch)
    postponed_task_switch = true;
}

/**
 * Requires TFW_SP_AFTER_HEAP or later.
 */
void InitIrql(void) {
    deferred_functions = PriorityQueueCreate(128, true, sizeof(struct irql_deferment));
    init_irql_done = true;
}