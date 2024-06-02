
#include <arch.h>
#include <irql.h>
#include <log.h>
#include <irq.h>
#include <thread.h>
#include <linkedlist.h>
#include <errno.h>
#include <process.h>
#include <ksignal.h>
#include <signal.h>
#include <panic.h>

#define HIGHEST_IRQ_NUM 256

static struct linked_list* irq_table[HIGHEST_IRQ_NUM] = {0};

int RegisterIrqHandler(int irq_num, irq_handler_t handler) {
    if (irq_num < 0 || irq_num >= HIGHEST_IRQ_NUM || handler == NULL) {
        return EINVAL;
    }

    if (irq_table[irq_num] == NULL) {
        irq_table[irq_num] = ListCreate();
    }

    ListInsertEnd(irq_table[irq_num], (void*)(size_t) handler);
    return 0; 
}

void RespondToIrq(int irq, int req_irl, platform_irq_context_t* context) {
    int irql = RaiseIrql(req_irl);
    ArchSendEoi(irq);
    
    if (irq_table[irq] != NULL) {
        struct linked_list_node* iter = ListGetFirstNode(irq_table[irq]);
        while (iter != NULL) {
            irq_handler_t handler = (irq_handler_t) (size_t) ListGetDataFromNode(iter);
            assert(handler != NULL);

            /*
             * Interrupt handlers return 0 if they handled the IRQ. Non-zero 
             * means 'leave this one for someone else'.
             */
            if (handler(context) == 0) {
                break;
            }

            iter = ListGetNextNode(iter);
        }
    }
    
    LowerIrql(irql);
}

void UnhandledFault(int type) {
    if (GetProcess() != NULL) {
        struct thread* thr = GetThread();
        LogWriteSerial("unhandled fault... terminating the thread 0x%X...\n", thr);

        /*
         * We 'schedule' so that we run the signal handler before we return from
         * this interrupt, which would take us back to a faulting instruction.
         */
        if (type == UNHANDLED_FAULT_SEGV && !(thr->blocked_signals & (1 << SIGSEGV))) {
            LogWriteSerial("raising signal SIGSEGV\n");
            RaiseSignal(GetThread(), SIGSEGV, false);
            Schedule();

        } else if (type == UNHANDLED_FAULT_ILL && !(thr->blocked_signals & (1 << SIGILL))) {
            LogWriteSerial("raising signal SIGILL\n");
            RaiseSignal(GetThread(), SIGILL, false);
            Schedule();

        } else if (type == UNHANDLED_FAULT_DIVISION && !(thr->blocked_signals & (1 << SIGFPE))) {
            LogWriteSerial("raising signal SIGFPE\n");
            RaiseSignal(GetThread(), SIGFPE, false);
            Schedule();

        } else {
            LogWriteSerial("bye bye thread\n");
            GetThread()->needs_termination = true;
        }

        
    } else {
        Panic(PANIC_UNHANDLED_KERNEL_EXCEPTION);
    }
}