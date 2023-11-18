#pragma once

#include <stdbool.h>

#define PIC_IRQ_BASE 32

void InitPic(void);
void SendPicEoi(int irq_num);
bool IsPicIrqSpurious(int irq_num);