#pragma once

#include <arch.h>

void RegisterIrqHandler(int irq_num, int(*handler)(platform_irq_context_t*));
void RespondToIrq(int irq_num, int required_irql);