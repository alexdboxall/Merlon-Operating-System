#pragma once

#include <common.h>

void InitSwapfile(void);

struct file* GetSwapfile(void);
uint64_t AllocSwap(void);
void DeallocSwap(uint64_t index);
int GetSwapCount(void);