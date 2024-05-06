
#include <panic.h>
#include <cpu.h>
#include <irql.h>
#include <log.h>
#include <priorityqueue.h>
#include <thread.h>
#include <assert.h>

struct deferment {
    void (*handler)(void*);
    void* context;
};

/**
 * Runs a function at an IRQL lower than or equal to the current IRQL. If the 
 * IRQLs match, the function will be run immediately. If the target IRQL is 
 * lower than the current IRQL, it will be deferred until the IRQL level drops 
 * below IRQL_SCHEDULER. Deferred function calls will be run in order from 
 * highest IRQL to lowest. If InitIrql() has not been called prior to calling 
 * this function, any requests meant to be deferred will instead be silently
 * ignored (this is needed to bootstrap the physical memory manager, et al.).
 */
void DeferUntilIrql(int irql, void(*handler)(void*), void* context) {
    struct cpu* cpu = GetCpu();
    int current = cpu->irql;
    if (irql == current || (irql == IRQL_STANDARD_HIGH_PRIORITY 
                              && current == IRQL_STANDARD)) {
        handler(context);

    } else if (irql > current) {
        PanicEx(PANIC_INVALID_IRQL, "invalid irql on DeferUntilIrql");

    } else if (cpu->init_irql_done) {
        struct deferment defer = {.context = context, .handler = handler};
        HeapAdtInsert(cpu->deferred_functions, (void*) &defer, irql);
    }
}

int GetIrql(void) {
    return GetCpu()->irql;
}

int RaiseIrql(int level) {
    ArchDisableInterrupts();

    struct cpu* cpu = GetCpu();
    int existing_level = cpu->irql;

    if (level < existing_level) {
        PanicEx(PANIC_INVALID_IRQL, "invalid irql on RaiseIrql");
    }

    cpu->irql = level;
    ArchSetIrql(level);

    return existing_level;
}

void LowerIrql(int target_level) {
    struct cpu* cpu = GetCpu();
    struct heap_adt* deferred_functions = cpu->deferred_functions;
    int current_level = cpu->irql;

    if (target_level > current_level) {
        PanicEx(PANIC_INVALID_IRQL, "invalid irql on LowerIrql");
    }

    while (cpu->init_irql_done && HeapAdtGetUsedSize(deferred_functions) > 0) {
        struct heap_adt_result next = HeapAdtPeek(deferred_functions);

        assert((int) next.priority <= current_level || 
            (next.priority == IRQL_STANDARD_HIGH_PRIORITY && current_level == IRQL_STANDARD));

        if ((int) next.priority >= target_level) {
            current_level = next.priority;
            if (current_level == IRQL_STANDARD_HIGH_PRIORITY) {
                current_level = IRQL_STANDARD;
            }
            
            /*
             * Must Pop() before we call the handler (otherwise if the handler 
             * does a raise/lower, it will retrigger itself and cause a 
             * recursion loop), and must also get data off the queue before we 
             * Pop(). Also must only actually lower the IRQL after doing this, 
             * so we don't get interrupted in between (as someone else could 
             * then Raise/Lower, and mess us up.)
             */
            struct deferment* deferred_call = (struct deferment*) next.data;
            void* context = deferred_call->context;
            void (*handler)(void*) = deferred_call->handler;
            if (handler == NULL) {
                ArchSetIrql(current_level);
                continue;
            }
            HeapAdtPop(deferred_functions);
            cpu->irql = current_level;
            ArchSetIrql(current_level);
            handler(context);

        } else {
            break;
        }
    }

    current_level = target_level;
    if (current_level == IRQL_STANDARD_HIGH_PRIORITY) {
        current_level = IRQL_STANDARD;
    }
    cpu->irql = current_level;
    ArchSetIrql(current_level);

    if (current_level == IRQL_STANDARD && cpu->postponed_task_switch) {
        cpu->postponed_task_switch = false;
        Schedule();
    }
}

void PostponeScheduleUntilStandardIrql(void) {
    GetCpu()->postponed_task_switch = true;
}

void InitIrql(void) {
    GetCpu()->deferred_functions = HeapAdtCreate(
        32, true, sizeof(struct deferment)
    );
    GetCpu()->init_irql_done = true;
}

int GetNumberInDeferQueue(void) {
    return HeapAdtGetUsedSize(GetCpu()->deferred_functions);
}