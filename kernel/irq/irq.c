
#include <arch.h>
#include <irql.h>
#include <log.h>
#include <irq.h>
#include <linkedlist.h>
#include <errno.h>

#define HIGHEST_IRQ_NUM 256

static struct linked_list* irq_table[HIGHEST_IRQ_NUM] = {0};

int RegisterIrqHandler(int irq_num, irq_handler_t handler) {
    (void) irq_num;
    (void) handler;

    if (irq_num < 0 || irq_num >= HIGHEST_IRQ_NUM || handler == NULL) {
        return EINVAL;
    }

    if (irq_table[irq_num] == NULL) {
        irq_table[irq_num] = LinkedListCreate();
    }

    LinkedListInsertEnd(irq_table[irq_num], (void*)(size_t) handler);
    return 0; 
}

/*
 * To be raised by the architecture specific code.
 *
 * @param required_irql The IRQL that this device handler needs to run at. Set to 0 if no change is needed.
 */
void RespondToIrq(int irq_num, int required_irql, platform_irq_context_t* context) {
    int irql = RaiseIrql(required_irql);
    ArchSendEoi(irq_num);     // must wait until we have raised

    struct linked_list_node* iter = LinkedListGetFirstNode(irq_table[irq_num]);
    while (iter != NULL) {
        irq_handler_t handler = (irq_handler_t)(size_t) LinkedListGetDataFromNode(iter);
        if (handler != NULL) {
            int result = handler(context);

            /*
             * Interrupt handlers return 0 if they could handle the IRQ (i.e. stop trying to handle it).
             * Non-zero means 'leave this one for someone else'.
             */
            if (result == 0) {
                break;
            }
        }

        iter = LinkedListGetNextNode(iter);
    }

    LowerIrql(irql);
}