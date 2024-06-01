#pragma once

#include <arch.h>
#include <common.h>

#define UNHANDLED_FAULT_SEGV    0
#define UNHANDLED_FAULT_ILL     1
#define UNHANDLED_FAULT_OTHER   2

typedef int(*irq_handler_t)(platform_irq_context_t*);

int RegisterIrqHandler(int irq_num, irq_handler_t handler);
void RespondToIrq(int irq_num, int required_irql, platform_irq_context_t* context);
void UnhandledFault(int type);