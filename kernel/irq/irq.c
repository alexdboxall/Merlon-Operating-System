
#include <arch.h>
#include <irql.h>
#include <log.h>

void RegisterIrqHandler(int irq_num, int(*handler)(platform_irq_context_t*)) {
    (void) irq_num;
    (void) handler;

    // TODO: !
}

/*
 * To be raised by the architecture specific code.
 *
 * @param required_irql The IRQL that this device handler needs to run at. Set to 0 if no change is needed.
 */
void RespondToIrq(int irq_num, int required_irql) {
    int irql = RaiseIrql(required_irql);
    ArchSendEoi(irq_num);     // must wait until we have raised

    // now that EOI is done, we can dispatch the interrupt handler

    LowerIrql(irql);
}