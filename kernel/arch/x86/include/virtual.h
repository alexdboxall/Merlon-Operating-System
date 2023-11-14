#pragma once

#include <stddef.h>

__attribute__((fastcall)) size_t x86KernelMemoryToPhysical(size_t virtual);