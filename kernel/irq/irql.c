
#include <panic.h>
#include <cpu.h>
#include <irql.h>
#include <log.h>
#include <priorityqueue.h>
#include <thread.h>
#include <assert.h>

struct irql_deferment {
    void (*handler)(void*);
    void* context;
};

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
 * 
 */
__attribute__((no_instrument_function)) void DeferUntilIrql(int irql, void(*handler)(void*), void* context) {
    if (irql == GetIrql()) {
        handler(context);

    } else if (irql > GetIrql()) {
        Panic(PANIC_INVALID_IRQL);

    } else {
        if (GetCpu()->init_irql_done) {
            struct irql_deferment deferment;
            deferment.context = context;
            deferment.handler = handler;
            PriorityQueueInsert(GetCpu()->deferred_functions, (void*) &deferment, irql);
        }
    }
}

/**
 * Returns the CPU's current IRQL.
 *
 * @return The IRQL.
 * @max_irql IRQL_HIGH
 */
__attribute__((no_instrument_function)) int GetIrql(void) {
    return GetCpu()->irql;
}

// Max IRQL: IRQL_HIGH
__attribute__((no_instrument_function)) int RaiseIrql(int level) {
    ArchDisableInterrupts();

    struct cpu* cpu = GetCpu();

    int existing_level = cpu->irql;

    if (level < existing_level) {
        Panic(PANIC_INVALID_IRQL);
    }

    cpu->irql = level;
    ArchSetIrql(level);

    return existing_level;
}

// Max IRQL: IRQL_HIGH
__attribute__((no_instrument_function)) void LowerIrql(int target_level) {
    // TODO: does this function need its own lock ? (e.g. for postponed_task_switch)    

    struct cpu* cpu = GetCpu();
    struct priority_queue* deferred_functions = cpu->deferred_functions;

    int current_level = cpu->irql;

    if (target_level > current_level) {
        Panic(PANIC_INVALID_IRQL);
    }

    while (cpu->init_irql_done && current_level != target_level && PriorityQueueGetUsedSize(deferred_functions) > 0) {
        struct priority_queue_result next = PriorityQueuePeek(deferred_functions);
        assert((int) next.priority <= current_level);

        if ((int) next.priority >= target_level) {
            current_level = next.priority;

            /*
             * Must Pop() before we call the handler (otherwise if the handler does a raise/lower, it will
             * retrigger itself and cause a recursion loop), and must also get data off the queue before we Pop().
             * Also must only actually lower the IRQL after doing this, so we don't get interrupted in between
             * (as someone else could then Raise/Lower, and mess us up.)
             */
            struct irql_deferment* deferred_call = (struct irql_deferment*) next.data;
            void* context = deferred_call->context;
            void (*handler)(void*) = deferred_call->handler;
            if (handler == NULL) {
                cpu->irql = current_level;
                ArchSetIrql(current_level);
                continue;
            }
            PriorityQueuePop(deferred_functions);
            cpu->irql = current_level;
            ArchSetIrql(current_level);
            handler(context);

        } else {
            break;
        }
    }

    current_level = target_level;
    cpu->irql = current_level;
    ArchSetIrql(current_level);

    if (current_level == IRQL_STANDARD && cpu->postponed_task_switch) {
        cpu->postponed_task_switch = false;
        Schedule();
    }
}

void PostponeScheduleUntilStandardIrql(void) {
    // TODO: does this function need its own lock ? (e.g. just for setting postponed_task_switch)
    GetCpu()->postponed_task_switch = true;
}

/**
 * Requires TFW_SP_AFTER_HEAP or later.
 */
void InitIrql(void) {
    GetCpu()->deferred_functions = PriorityQueueCreate(128, true, sizeof(struct irql_deferment));
    GetCpu()->init_irql_done = true;
}