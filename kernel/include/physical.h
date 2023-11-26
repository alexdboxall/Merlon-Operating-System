#pragma once

#include <stddef.h>

void DeallocPhys(size_t addr);
void DeallocPhysContiguous(size_t addr, size_t bytes);
size_t AllocPhys(void);
size_t AllocPhysContiguous(size_t bytes, size_t min_addr, size_t max_addr, size_t boundary);
void FoundPhysPage(size_t addr);
void InitPhys(void);
void ReinitPhys(void);

size_t GetTotalPhysKilobytes(void);
size_t GetFreePhysKilobytes(void);