#pragma once

#include <common.h>

export void DeallocPhys(size_t addr);
export void DeallocPhysContiguous(size_t addr, size_t bytes);
export size_t AllocPhys(void);
export size_t AllocPhysContiguous(size_t bytes, size_t min_addr, size_t max_addr, size_t boundary);
export size_t GetTotalPhysKilobytes(void);
export size_t GetFreePhysKilobytes(void);

void InitPhys(void);
void ReinitPhys(void);

