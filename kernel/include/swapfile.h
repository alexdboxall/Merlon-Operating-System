#pragma once

#include <common.h>

void InitSwapfile(void);

struct open_file* GetSwapfile(void);
uint64_t AllocateSwapfileIndex(void);
void DeallocateSwapfileIndex(uint64_t index);
int GetNumberOfPagesOnSwapfile(void);